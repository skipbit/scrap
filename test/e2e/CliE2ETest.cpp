// End-to-end tests that spawn the built `scrap` binary as a real subprocess
// and assert on its externally observable behavior (exit code, stdout,
// stderr). These are black-box tests: no headers from src/ are used here.

#include "digest/Sha256.h"
#include "support/FakeOciRegistry.h"
#include "support/HttpTestServer.h"
#include "support/StoredXz.h"
#include "support/TarArchiveBuilder.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <poll.h>
#include <regex>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

#ifndef SCRAP_BINARY_PATH
#error "SCRAP_BINARY_PATH must be defined by the build (path to the scrap executable)"
#endif

namespace {

/**
 * The release triple of a version banner: the program name, a release triple,
 * and an optional parenthesised description of the build. Nothing when text
 * is not a banner.
 *
 * The check is structural on purpose. Comparing against the exact string the
 * build produced would put the same value on both sides of the assertion, so
 * no wrong derivation could fail it.
 */
std::optional<std::string> releaseInBanner(const std::string& text)
{
    static const std::regex Pattern{ R"(^scrap ([0-9]+\.[0-9]+\.[0-9]+)( \(.+\))?$)" };
    std::smatch match;
    if (! std::regex_match(text, match, Pattern)) {
        return std::nullopt;
    }
    return match[1].str();
}

bool isVersionBanner(const std::string& text)
{
    return releaseInBanner(text).has_value();
}

/**
 * How many processors a build started from here may use, found as scrap
 * finds it: the processors this thread may run on, where the system says.
 */
unsigned processorsForTheBuild()
{
#if defined(__linux__)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    return (::sched_getaffinity(0, sizeof(allowed), &allowed) == 0) ? static_cast<unsigned>(CPU_COUNT(&allowed)) : 1;
#else
    return std::thread::hardware_concurrency();
#endif
}

/// A manifest that loads without error.
constexpr std::string_view ValidManifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

/// A manifest whose empty declaration states that the project builds nothing.
constexpr std::string_view ManifestWithoutTargets = "bin = []\n\n[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

/// The source the default layout expects, which gives a project one target.
constexpr std::string_view MainSource = "int main() { return 0; }\n";

/// Two executables that each print their own name, and a source they share.
constexpr std::string_view TwoExecutablesManifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n"
                                                    "[[bin]]\nname = \"app\"\nsrc = \"src/main.cpp\"\n\n"
                                                    "[[bin]]\nname = \"tool\"\nsrc = \"src/tool.cpp\"\n";

/// A library beside an executable that uses it.
constexpr std::string_view LibraryAndExecutableManifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n"
                                                          "[[bin]]\nname = \"app\"\nsrc = \"src/main.cpp\"\n\n"
                                                          "[[lib]]\nname = \"core\"\n";

/// A program that writes the profile it was compiled for, as NDEBUG tells it.
constexpr std::string_view ProfileSource = "#include <cstdio>\n"
                                           "int main()\n"
                                           "{\n"
                                           "#ifdef NDEBUG\n"
                                           "    std::puts(\"release\");\n"
                                           "#else\n"
                                           "    std::puts(\"debug\");\n"
                                           "#endif\n"
                                           "}\n";

/**
 * Whether @p directory or one of its parents holds a scrap.toml.
 *
 * The tests that expect no project depend on the temp location not sitting
 * inside one, and skip instead of failing on a machine where it does.
 */
bool insideAProject(const std::filesystem::path& directory)
{
    std::error_code ec;
    for (std::filesystem::path current = directory;; current = current.parent_path()) {
        if (std::filesystem::is_regular_file(current / "scrap.toml", ec)) {
            return true;
        }
        if (current.parent_path() == current) {
            return false;
        }
    }
}

constexpr std::chrono::milliseconds HarnessTimeout{ 5000 };

/// A real compiler takes longer than the commands that only read files, and
/// longer again on a loaded machine.
constexpr std::chrono::milliseconds BuildTimeout{ 120000 };
constexpr std::size_t ReadChunkBytes = 4096;

/**
 * Result of running the scrap binary once.
 */
struct ProcessOutput {
    bool exitedNormally = false;
    int exitCode = -1;
    int signal = 0;  ///< The signal that ended the process, or 0.
    std::string stdoutText;
    std::string stderrText;
};

/**
 * Remove leading/trailing ASCII whitespace from @p text.
 */
std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    while ((begin < text.size()) && (std::isspace(static_cast<unsigned char>(text[begin])) != 0)) {
        ++begin;
    }
    std::size_t end = text.size();
    while ((end > begin) && (std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)) {
        --end;
    }
    return std::string{ text.substr(begin, end - begin) };
}

/**
 * Build the child's environment: a minimal, fixed base (SCRAP_HOME, PATH,
 * LC_ALL) so external-command discovery only ever sees this test's fixture
 * directory, plus any caller-supplied "KEY=VALUE" overrides.
 */
std::vector<std::string> buildEnv(const std::filesystem::path& scrapHome, const std::vector<std::string>& overrides)
{
    std::vector<std::string> env{
        "SCRAP_HOME=" + scrapHome.string(),
        "PATH=/usr/bin:/bin",
        "LC_ALL=C",
    };

    for (const auto& entry : overrides) {
        auto eq = entry.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        auto key = entry.substr(0, eq);

        bool replaced = false;
        for (auto& existing : env) {
            auto existingEq = existing.find('=');
            if ((existingEq != std::string::npos) && (existing.compare(0, existingEq, key) == 0)) {
                existing = entry;
                replaced = true;
                break;
            }
        }
        if (! replaced) {
            env.push_back(entry);
        }
    }

    return env;
}

/**
 * Drain both @p fd1 and @p fd2 into @p out1 / @p out2 until each reaches
 * EOF or @p deadline passes. Polling both fds together avoids the deadlock
 * that reading them sequentially could cause if the child fills one pipe
 * while blocked writing to the other.
 *
 * @return true if @p deadline was reached before both fds closed.
 */
bool drainBoth(int fd1, int fd2, std::chrono::steady_clock::time_point deadline, std::string& out1, std::string& out2)
{
    std::array<char, ReadChunkBytes> buffer{};
    bool open1 = true;
    bool open2 = true;

    while (open1 || open2) {
        auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return true;
        }
        auto remainingMs = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();

        std::array<struct pollfd, 2> pfds{};
        int count = 0;
        int idx1 = -1;
        int idx2 = -1;
        if (open1) {
            idx1 = count;
            pfds[static_cast<std::size_t>(count)] = { .fd = fd1, .events = POLLIN, .revents = 0 };
            ++count;
        }
        if (open2) {
            idx2 = count;
            pfds[static_cast<std::size_t>(count)] = { .fd = fd2, .events = POLLIN, .revents = 0 };
            ++count;
        }

        int pollResult = ::poll(pfds.data(), static_cast<nfds_t>(count), static_cast<int>(remainingMs));
        if (pollResult == 0) {
            return true;
        }
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX poll() revents flag combination
        if ((idx1 >= 0) && ((pfds[static_cast<std::size_t>(idx1)].revents & (POLLIN | POLLHUP)) != 0)) {
            auto bytesRead = ::read(fd1, buffer.data(), buffer.size());
            if (bytesRead == 0) {
                open1 = false;
            } else if (bytesRead < 0) {
                if ((errno != EINTR) && (errno != EAGAIN)) {
                    open1 = false;
                }
            } else {
                out1.append(buffer.data(), static_cast<std::size_t>(bytesRead));
            }
        }
        // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX poll() revents flag combination
        if ((idx2 >= 0) && ((pfds[static_cast<std::size_t>(idx2)].revents & (POLLIN | POLLHUP)) != 0)) {
            auto bytesRead = ::read(fd2, buffer.data(), buffer.size());
            if (bytesRead == 0) {
                open2 = false;
            } else if (bytesRead < 0) {
                if ((errno != EINTR) && (errno != EAGAIN)) {
                    open2 = false;
                }
            } else {
                out2.append(buffer.data(), static_cast<std::size_t>(bytesRead));
            }
        }
    }

    return false;
}

}  // namespace

/**
 * Fixture providing a per-test temp directory (with a bin/ subdirectory for
 * dummy external commands) and a helper to run the built scrap binary.
 */
class CliE2ETest : public ::testing::Test {
protected:
    /**
     * Create a per-test temp directory, named after the test so that one
     * left behind by a killed test can be traced back to it.
     */
    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        _temp.emplace(std::string("scrap_e2e_") + info->name());
        // An empty path would put the fixture's files in the working directory.
        ASSERT_FALSE(_temp->path().empty());
        _root = _temp->path();
        _temp->makeDirectory("bin");
    }

    void TearDown() override
    {
        _temp.reset();
    }

    /**
     * Write an executable dummy script named @p name into the fixture's
     * bin/ directory, with @p body as the shell script content.
     */
    void makeDummy(const std::string& name, const std::string& body) const
    {
        const auto path = _temp->writeFile("bin/" + name, "#!/bin/sh\n" + body + "\n");
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    }

    /**
     * Make the named pipe "meeting" in the fixture directory. Opening it
     * waits until it is open at the other end too, so two programs that open
     * it meet there.
     */
    void makeMeetingPoint() const
    {
        ASSERT_EQ(::mkfifo((_root / "meeting").c_str(), S_IRUSR | S_IWUSR), 0) << std::strerror(errno);
    }

    /**
     * Let go of a program still waiting at the meeting point, as one is when
     * the build did not run the compilations at once and was stopped: opening
     * each end in turn, without waiting, ends the wait at the other.
     */
    void releaseMeetingPoint() const
    {
        const std::filesystem::path meeting = _root / "meeting";
        // NOLINTBEGIN(hicpp-signed-bitwise) - POSIX open() flag combination
        const int reader = ::open(meeting.c_str(), O_RDONLY | O_NONBLOCK);
        const int writer = ::open(meeting.c_str(), O_WRONLY | O_NONBLOCK);
        // NOLINTEND(hicpp-signed-bitwise)
        for (const int end : { writer, reader }) {
            if (end >= 0) {
                ::close(end);
            }
        }
    }

    /**
     * A CXX override naming a program that can be run.
     *
     * A test about something else should not depend on the host having a
     * compiler in the fixed PATH the harness gives the child.
     */
    std::string dummyCompiler() const
    {
        makeDummy("dummy-c++", "exit 0");
        return "CXX=" + (std::filesystem::canonical(_root) / "bin" / "dummy-c++").string();
    }

    /**
     * Write @p content to @p relative below the fixture directory, creating
     * parent directories.
     */
    void writeFile(const std::filesystem::path& relative, std::string_view content) const
    {
        _temp->writeFile(relative.string(), content);
    }

    /**
     * Lay out @p family @p version below the fixture's SCRAP_HOME as an
     * install would, for every host scrap is released for, so the test
     * holds wherever it runs. Its compiler is an empty file.
     */
    void installToolchain(std::string_view family, std::string_view version) const
    {
        for (const char* host : { "linux-x86_64", "linux-aarch64", "darwin-x86_64", "darwin-aarch64" }) {
            const std::string directory = "toolchains/" + std::string{ family } + "/" + std::string{ version } + "/" + host;
            _temp->writeFile(directory + "/bin/g++", "");
            _temp->writeFile(directory + "/.scrap-toolchain.toml", "compiler = \"bin/g++\"\n");
        }
    }

    /**
     * Install @p family @p version as installToolchain() does, with a
     * compiler that adds a line naming the toolchain to compilers-used and
     * its arguments, one a line, to compiler-arguments, and then runs the
     * compiler this test binary was built with, so a build with it succeeds
     * and says which toolchain it used and how.
     */
    void installWorkingToolchain(std::string_view family, std::string_view version) const
    {
        installToolchain(family, version);
        const std::string name = std::string{ family } + "@" + std::string{ version };
        const std::string body = "#!/bin/sh\necho '" + name + "' >> '" + (_root / "compilers-used").string() + "'\nprintf '%s\\n' \"$@\" >> '"
                                 + (_root / "compiler-arguments").string() + "'\nexec '" + SCRAP_TEST_CXX + "' \"$@\"\n";
        for (const char* host : { "linux-x86_64", "linux-aarch64", "darwin-x86_64", "darwin-aarch64" }) {
            const auto compiler = _temp->writeFile("toolchains/" + std::string{ family } + "/" + std::string{ version } + "/" + host + "/bin/g++", body);
            std::filesystem::permissions(compiler, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        }
    }

    /**
     * Record @p directory as where the runtime libraries of @p family
     * @p version, laid out by installToolchain(), are, for every host.
     */
    void recordRuntimeLibraryDirectory(std::string_view family, std::string_view version, std::string_view directory) const
    {
        for (const char* host : { "linux-x86_64", "linux-aarch64", "darwin-x86_64", "darwin-aarch64" }) {
            const std::string toolchain = "toolchains/" + std::string{ family } + "/" + std::string{ version } + "/" + host;
            _temp->writeFile(toolchain + "/" + std::string{ directory } + "/.keep", "");
            _temp->writeFile(toolchain + "/.scrap-toolchain.toml",
                             "compiler = \"bin/g++\"\nruntime-library-dirs = [\"" + std::string{ directory } + "\"]\n");
        }
    }

    /**
     * Record @p compileArgument and @p linkArgument as what @p family
     * @p version, laid out by installToolchain(), needs to compile and to
     * link, for every host.
     */
    void recordToolchainArguments(std::string_view family, std::string_view version, std::string_view compileArgument, std::string_view linkArgument) const
    {
        for (const char* host : { "linux-x86_64", "linux-aarch64", "darwin-x86_64", "darwin-aarch64" }) {
            const std::string toolchain = "toolchains/" + std::string{ family } + "/" + std::string{ version } + "/" + host;
            _temp->writeFile(toolchain + "/.scrap-toolchain.toml",
                             "compiler = 'bin/g++'\ncompile-args = ['" + std::string{ compileArgument } + "']\nlink-args = ['" + std::string{ linkArgument } + "']\n");
        }
    }

    /**
     * The toolchains the compilers installWorkingToolchain() lays out have
     * run as, one line each, in order.
     */
    [[nodiscard]] std::string compilersUsed() const
    {
        return readFile("compilers-used");
    }

    /**
     * The contents of @p relative below the fixture directory, empty when it
     * cannot be read.
     */
    [[nodiscard]] std::string readFile(const std::filesystem::path& relative) const
    {
        return _temp->readFile(relative);
    }

    /**
     * The compiler this test binary was built with, as a CXX override. The
     * fixed PATH the harness gives the child holds no compiler of its own,
     * and a build has to reach one the platform actually provides.
     */
    [[nodiscard]] static std::string realCompiler()
    {
        return std::string{ "CXX=" } + SCRAP_TEST_CXX;
    }

    /**
     * Run the built scrap binary with @p args, a minimal controlled
     * environment (SCRAP_HOME=_root, plus any @p envOverrides), and the
     * given @p cwd. Captures stdout/stderr separately.
     */
    [[nodiscard]] ProcessOutput runScrap(const std::vector<std::string>& args,
                                         const std::vector<std::string>& envOverrides,
                                         const std::filesystem::path& cwd,
                                         std::chrono::milliseconds timeout = HarnessTimeout) const
    {
        std::vector<std::string> command{ SCRAP_BINARY_PATH };
        command.insert(command.end(), args.begin(), args.end());
        return runProgram(command, envOverrides, cwd, timeout);
    }

    /**
     * Run @p command, the first of which names the program, in the same
     * controlled environment the scrap binary is run in.
     */
    [[nodiscard]] ProcessOutput runProgram(const std::vector<std::string>& command,
                                           const std::vector<std::string>& envOverrides,
                                           const std::filesystem::path& cwd,
                                           std::chrono::milliseconds timeout = HarnessTimeout) const
    {
        std::array<int, 2> outPipe{ -1, -1 };
        std::array<int, 2> errPipe{ -1, -1 };
        if (::pipe(outPipe.data()) != 0) {
            return {};
        }
        if (::pipe(errPipe.data()) != 0) {
            ::close(outPipe[0]);
            ::close(outPipe[1]);
            return {};
        }

        std::vector<std::string> ownedArgs = command;
        std::vector<char*> argv;
        argv.reserve(ownedArgs.size() + 1);
        for (auto& arg : ownedArgs) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        auto ownedEnv = buildEnv(_root, envOverrides);
        std::vector<char*> envp;
        envp.reserve(ownedEnv.size() + 1);
        for (auto& entry : ownedEnv) {
            envp.push_back(entry.data());
        }
        envp.push_back(nullptr);

        const std::string cwdStr = cwd.string();

        pid_t childPid = ::fork();
        if (childPid < 0) {
            ::close(outPipe[0]);
            ::close(outPipe[1]);
            ::close(errPipe[0]);
            ::close(errPipe[1]);
            return {};
        }

        if (childPid == 0) {
            // Child: wire up stdout/stderr, isolate stdin, chdir, then exec.
            ::dup2(outPipe[1], STDOUT_FILENO);
            ::dup2(errPipe[1], STDERR_FILENO);
            ::close(outPipe[0]);
            ::close(outPipe[1]);
            ::close(errPipe[0]);
            ::close(errPipe[1]);

            int devNull = ::open("/dev/null", O_RDONLY);
            if (devNull >= 0) {
                ::dup2(devNull, STDIN_FILENO);
                ::close(devNull);
            }

            if (::chdir(cwdStr.c_str()) != 0) {
                _exit(127);
            }

            // A process a test ends with a quit leaves no core file behind.
            const rlimit noCore{ .rlim_cur = 0, .rlim_max = 0 };
            ::setrlimit(RLIMIT_CORE, &noCore);

            ::execve(argv[0], argv.data(), envp.data());
            _exit(127);  // execve() only returns on failure.
        }

        // Parent: close the write ends so EOF is observed once the child exits.
        ::close(outPipe[1]);
        ::close(errPipe[1]);

        ProcessOutput result;
        auto deadline = std::chrono::steady_clock::now() + timeout;
        bool timedOut = drainBoth(outPipe[0], errPipe[0], deadline, result.stdoutText, result.stderrText);
        ::close(outPipe[0]);
        ::close(errPipe[0]);

        if (timedOut) {
            ::kill(childPid, SIGKILL);
        }

        int status = 0;
        pid_t waited = -1;
        do {
            waited = ::waitpid(childPid, &status, 0);
        } while ((waited < 0) && (errno == EINTR));

        if ((! timedOut) && (waited == childPid) && WIFEXITED(status)) {
            result.exitedNormally = true;
            result.exitCode = WEXITSTATUS(status);
        }
        if ((! timedOut) && (waited == childPid) && WIFSIGNALED(status)) {
            result.signal = WTERMSIG(status);
        }

        return result;
    }

    std::optional<scrap::TestSupport::TempDirectory> _temp;
    std::filesystem::path _root;
};

// --- TS-01: builtin resolve + execute ------------------------------------------

TEST_F(CliE2ETest, BuiltinResolveExecute)
{
    auto result = runScrap({ "version" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(isVersionBanner(trim(result.stdoutText))) << "banner: " << trim(result.stdoutText);
    EXPECT_TRUE(result.stderrText.empty());
}

// --- TS-02: external command discovery -----------------------------------------

TEST_F(CliE2ETest, ExternalDiscovery)
{
    makeDummy("scrap-greet", R"(case "$1" in
  --scrap-metadata) echo "Greet people" ;;
  --help) echo "greet - Greet people" ;;
  *) echo "greet called" ;;
esac)");

    auto result = runScrap({ "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("External Commands"), std::string::npos);
    EXPECT_NE(result.stdoutText.find("greet"), std::string::npos);
}

/**
 * An external command whose name holds a dot is taken as the command it
 * names, as one without a dot is, also where a nested command has the parts
 * as names.
 */
TEST_F(CliE2ETest, ExternalNameWithADotIsTakenAsOneName)
{
    makeDummy("scrap-greet", "echo \"greet called\"");
    auto plain = runScrap({ "greet" }, {}, _root);

    for (const std::string name : { "greet.sh", "toolchain.list" }) {
        makeDummy("scrap-" + name, "echo \"" + name + " called\"");

        auto dotted = runScrap({ name }, {}, _root);

        ASSERT_TRUE(dotted.exitedNormally) << name;
        EXPECT_EQ(dotted.exitCode, plain.exitCode) << name;
        EXPECT_EQ(dotted.stdoutText, plain.stdoutText) << name;
        std::string expected = plain.stderrText;
        const auto position = expected.find("greet");
        ASSERT_NE(position, std::string::npos) << plain.stderrText;
        expected.replace(position, std::string_view{ "greet" }.size(), name);
        EXPECT_EQ(dotted.stderrText, expected) << name;
    }
}

/**
 * A word joined by a dot names no nested command and no option of one; it
 * is refused as the word it is, whatever else is wrong after it.
 */
TEST_F(CliE2ETest, DottedWordNamesNoNestedCommand)
{
    const std::vector<std::pair<std::vector<std::string>, std::string>> commandLines{
        { { "toolchain.list" }, "toolchain.list" },
        { { "--build.release" }, "--build.release" },
        { { "toolchain.list", "extra" }, "toolchain.list" },
    };
    for (const auto& [args, word] : commandLines) {
        auto result = runScrap(args, {}, _root);

        ASSERT_TRUE(result.exitedNormally) << word;
        EXPECT_EQ(result.exitCode, 2) << word;
        EXPECT_TRUE(result.stdoutText.empty()) << word << ": " << result.stdoutText;
        EXPECT_EQ(result.stderrText, "The following argument was not expected: " + word + "\nRun 'scrap --help' for usage information.\n");
    }
}

/**
 * Help asked for a dotted word answers as help does for that word.
 */
TEST_F(CliE2ETest, HelpForADottedWordAnswersAsHelpDoes)
{
    auto asked = runScrap({ "toolchain.list", "--help" }, {}, _root);
    auto help = runScrap({ "help", "toolchain.list" }, {}, _root);

    ASSERT_TRUE(asked.exitedNormally);
    EXPECT_EQ(asked.exitCode, 2);
    EXPECT_EQ(asked.exitCode, help.exitCode);
    EXPECT_EQ(asked.stdoutText, help.stdoutText);
    EXPECT_EQ(asked.stderrText, help.stderrText);
}

/**
 * An external command whose name the parser cannot hold is left out with a
 * warning, so the other commands and the help still work.
 */
TEST_F(CliE2ETest, ExternalNameTheParserCannotHoldIsLeftOut)
{
    makeDummy("scrap-a:b", "echo \"a:b called\"");
    const auto warning = "warning: command 'a:b' cannot be used as a command name; ignoring " + (_root / "bin" / "scrap-a:b").string() + "\n";

    auto version = runScrap({ "version" }, {}, _root);
    auto help = runScrap({ "--help" }, {}, _root);

    ASSERT_TRUE(version.exitedNormally);
    EXPECT_EQ(version.exitCode, 0);
    EXPECT_TRUE(isVersionBanner(trim(version.stdoutText))) << "stdout: " << version.stdoutText;
    EXPECT_EQ(version.stderrText, warning);
    ASSERT_TRUE(help.exitedNormally);
    EXPECT_EQ(help.exitCode, 0);
    EXPECT_EQ(help.stdoutText.find("a:b"), std::string::npos) << help.stdoutText;
    EXPECT_EQ(help.stderrText, warning);
}

/**
 * Each name the parser cannot hold is left out the same way, and the warning
 * shows a control character as text.
 */
TEST_F(CliE2ETest, EveryExternalNameTheParserCannotHoldIsLeftOut)
{
    const std::vector<std::pair<std::string, std::string>> names{
        { "a=b", "a=b" },
        { "a b", "a b" },
        { "a{b", "a{b" },
        { "!x", "!x" },
        { "-x", "-x" },
        { "", "" },
        { std::string{ "a" } + '\x01' + "b", "a\\x01b" },
    };
    for (const auto& [name, shown] : names) {
        makeDummy("scrap-" + name, "exit 0");

        auto result = runScrap({ "version" }, {}, _root);

        ASSERT_TRUE(result.exitedNormally) << shown;
        EXPECT_EQ(result.exitCode, 0) << shown;
        EXPECT_NE(result.stderrText.find("warning: command '" + shown + "' cannot be used as a command name; ignoring "), std::string::npos) << result.stderrText;
        std::filesystem::remove(_root / "bin" / ("scrap-" + name));
    }
}

// --- TS-03: project scope with no scrap.toml (honest scope: graceful only) ----

TEST_F(CliE2ETest, ProjectScopeNoConfig)
{
    // No scrap.toml is created in _root. StubScriptsReader always returns an
    // empty script list regardless of project contents, so this only proves
    // a config-less project run does not crash and still lists builtins -
    // it does NOT exercise dynamic [scripts] parsing. That is covered by the
    // unit-level ProjectCommandResolverTest against a real ScriptsReader.
    auto result = runScrap({ "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("Built-in Commands"), std::string::npos);
    EXPECT_EQ(result.stdoutText.find("totally-fake-project-script"), std::string::npos);
}

/**
 * An external command is named by its filename, which can hold any byte, so
 * every line that names one shows a byte a terminal acts on as text: the
 * warning for a name found twice, and the line for a command not available.
 */
TEST_F(CliE2ETest, ExternalNameIsShownAsText)
{
    // U+009B, CSI among the C1 control characters. Every file system the
    // tests run on can hold it, since it is well-formed UTF-8.
    const std::string name = "\xc2\x9b"
                             "x";
    makeDummy("scrap-" + name, "exit 0");
    const auto other = _temp->writeFile("other/scrap-" + name, "#!/bin/sh\nexit 0\n");
    std::filesystem::permissions(other, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

    auto result = runScrap({ name }, { "PATH=" + (_root / "other").string() + ":/usr/bin:/bin" }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "warning: command '\\xC2\\x9Bx' already registered; ignoring duplicate\n"
              "Command '\\xC2\\x9Bx' is not available yet.\n"
              "Run 'scrap --help' for usage information.\n");
}

// --- TS-04: builtin vs external name collision priority ------------------------

TEST_F(CliE2ETest, Priority)
{
    makeDummy("scrap-greet", "echo \"greet called\"");
    makeDummy("scrap-version", "echo \"external version\"");

    auto result = runScrap({ "version" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(isVersionBanner(trim(result.stdoutText))) << "stdout: " << result.stdoutText;
    EXPECT_NE(result.stderrText.find("warning: command 'version' already registered; ignoring duplicate"), std::string::npos);
}

// --- TS-05: global help integration ---------------------------------------------

TEST_F(CliE2ETest, HelpIntegration)
{
    auto result = runScrap({ "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    for (const auto* expected : { "USAGE: scrap",
                                  "Built-in Commands",
                                  "Project Commands",
                                  "Toolchain Commands",
                                  "Template Commands",
                                  "See 'scrap help <command>'" }) {
        EXPECT_NE(result.stdoutText.find(expected), std::string::npos) << "missing: " << expected;
    }
}

// --- help command without an argument -------------------------------------------

TEST_F(CliE2ETest, HelpWithoutArgumentListsCommands)
{
    auto result = runScrap({ "help" }, {}, _root);
    auto flag = runScrap({ "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_NE(result.stdoutText.find("USAGE: scrap"), std::string::npos) << result.stdoutText;
    EXPECT_EQ(result.stdoutText, flag.stdoutText);
}

TEST_F(CliE2ETest, HelpForACommandShowsItsUsage)
{
    auto result = runScrap({ "help", "build" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("USAGE: scrap build [OPTIONS] [path]"), std::string::npos) << result.stdoutText;
    EXPECT_NE(result.stdoutText.find("--release"), std::string::npos) << result.stdoutText;
    EXPECT_NE(result.stdoutText.find("Build with the release profile\n"), std::string::npos) << result.stdoutText;
}

TEST_F(CliE2ETest, HelpForAnUnknownCommandFails)
{
    auto result = runScrap({ "help", "nosuch" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "error: unknown command 'nosuch'\nhint: run 'scrap --help' to list the commands\n");
}

TEST_F(CliE2ETest, HelpForAnEmptyCommandNameFails)
{
    auto result = runScrap({ "help", "" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "error: unknown command ''\nhint: run 'scrap --help' to list the commands\n");
}

/**
 * The name reported back is written as text: an escape sequence in it reaches
 * the terminal as \xNN rather than as an instruction to act on, while letters
 * stay as the user typed them.
 */
TEST_F(CliE2ETest, HelpForAnUnknownCommandWritesTheNameAsText)
{
    // The name holds U+65E5 and an escape sequence that names a terminal.
    auto result = runScrap({ "help", "\xe6\x97\xa5x\x1b]0;pwn\x07" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: unknown command '\xe6\x97\xa5x\\x1B]0;pwn\\x07'"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find('\x1b'), std::string::npos);
    EXPECT_EQ(result.stderrText.find('\x07'), std::string::npos);
}

TEST_F(CliE2ETest, HelpForANestedCommandShowsItsUsage)
{
    auto result = runScrap({ "toolchain", "list", "--help" }, {}, _root);
    auto shortFlag = runScrap({ "template", "update", "-h" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "USAGE: scrap toolchain list\n\nDisplay installed toolchains\n");
    ASSERT_TRUE(shortFlag.exitedNormally);
    EXPECT_EQ(shortFlag.exitCode, 0);
    EXPECT_EQ(shortFlag.stdoutText, "USAGE: scrap template update\n\nUpdate template sources\n");
}

TEST_F(CliE2ETest, HelpForANestedCommandTakesItsWords)
{
    auto result = runScrap({ "help", "toolchain", "list" }, {}, _root);
    auto flag = runScrap({ "toolchain", "list", "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "USAGE: scrap toolchain list\n\nDisplay installed toolchains\n");
    EXPECT_EQ(result.stdoutText, flag.stdoutText);
}

TEST_F(CliE2ETest, HelpShowsItTakesSeveralWords)
{
    auto result = runScrap({ "help", "help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("USAGE: scrap help [command]...\n"), std::string::npos) << result.stdoutText;
}

TEST_F(CliE2ETest, HelpForAnUnknownNestedCommandFails)
{
    auto result = runScrap({ "help", "toolchain", "nosuch" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "error: unknown command 'toolchain nosuch'\nhint: run 'scrap toolchain --help' to list its commands\n");
}

TEST_F(CliE2ETest, HelpPastACommandWithoutSubcommandsFails)
{
    auto result = runScrap({ "help", "clean", "nosuch" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "error: unknown command 'clean nosuch'\nhint: run 'scrap clean --help' for its usage\n");
}

TEST_F(CliE2ETest, HelpFlagAfterAnUnknownNestedCommandFails)
{
    auto before = runScrap({ "toolchain", "nosuch", "--help" }, {}, _root);
    auto after = runScrap({ "toolchain", "--help", "nosuch" }, {}, _root);

    for (const auto& result : { before, after }) {
        ASSERT_TRUE(result.exitedNormally);
        EXPECT_EQ(result.exitCode, 2);
        EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
        EXPECT_EQ(result.stderrText, "error: unknown command 'toolchain nosuch'\nhint: run 'scrap toolchain --help' to list its commands\n");
    }
}

TEST_F(CliE2ETest, HelpFlagAfterAnUnknownWordBeforeACommandFails)
{
    auto result = runScrap({ "toolchain", "nosuch", "list", "--help" }, {}, _root);
    auto words = runScrap({ "help", "toolchain", "nosuch", "list" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "error: unknown command 'toolchain nosuch list'\nhint: run 'scrap toolchain --help' to list its commands\n");
    EXPECT_EQ(result.stderrText, words.stderrText);
}

TEST_F(CliE2ETest, HelpFlagPastACommandWithoutSubcommandsFails)
{
    auto result = runScrap({ "toolchain", "list", "nosuch", "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "error: unknown command 'toolchain list nosuch'\nhint: run 'scrap toolchain list --help' for its usage\n");
}

TEST_F(CliE2ETest, HelpFlagAfterAnUnknownCommandFails)
{
    auto result = runScrap({ "nosuch", "--help" }, {}, _root);
    auto words = runScrap({ "help", "nosuch" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, words.stderrText);
}

TEST_F(CliE2ETest, HelpFlagAfterAPositionalShowsTheCommand)
{
    auto result = runScrap({ "clean", "somepath", "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_NE(result.stdoutText.find("USAGE: scrap clean [path]\n"), std::string::npos) << result.stdoutText;
}

TEST_F(CliE2ETest, HelpForACommandWithSubcommandsListsThem)
{
    auto result = runScrap({ "toolchain", "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("USAGE: scrap toolchain <COMMAND>\n"), std::string::npos) << result.stdoutText;
    EXPECT_NE(result.stdoutText.find("    list "), std::string::npos) << result.stdoutText;
}

// --- TS-06: unknown command -------------------------------------------------------

TEST_F(CliE2ETest, UnknownCommand)
{
    auto result = runScrap({ "nonexistent" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_NE(result.stderrText.find("Run 'scrap --help' for usage information"), std::string::npos);
}

/**
 * The parser writes the argument it did not expect into its message, and that
 * message is written as text.
 */
TEST_F(CliE2ETest, AnUnexpectedArgumentIsWrittenAsText)
{
    // The argument holds U+65E5 and an escape sequence that names a terminal.
    auto result = runScrap({ "new", "hello", "\xe6\x97\xa5x\x1b]0;pwn\x07" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("\xe6\x97\xa5x\\x1B]0;pwn\\x07"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find('\x1b'), std::string::npos);
    EXPECT_EQ(result.stderrText.find('\x07'), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(_root / "hello"));
}

/**
 * A usage error in the command line ends with status 2, whether the parser
 * or help finds it.
 */
TEST_F(CliE2ETest, UsageErrorsInTheCommandLineEndWithStatus2)
{
    const std::vector<std::vector<std::string>> commandLines{
        {},
        { "nosuch" },
        { "toolchain", "nosuch" },
        { "build", "--bogus" },
        { "new" },
        { "version", "--", "a" },
        { "help", "nosuch" },
        { "nosuch", "--help" },
        { "toolchain.list" },
        { "--build.release" },
        { "toolchain.list", "--help" },
        { "--toolchain.help" },
    };
    for (const auto& args : commandLines) {
        std::string typed = "scrap";
        for (const auto& arg : args) {
            typed += " " + arg;
        }

        auto result = runScrap(args, {}, _root);

        ASSERT_TRUE(result.exitedNormally) << typed << ": " << result.stderrText;
        EXPECT_EQ(result.exitCode, 2) << typed << ": " << result.stderrText;
        EXPECT_TRUE(result.stdoutText.empty()) << typed << ": " << result.stdoutText;
    }
}

// --- TS-07: real CLI11 nested subcommand parsing --------------------------------

TEST_F(CliE2ETest, RealCli11Nested)
{
    auto result = runScrap({ "toolchain", "list" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("list: not yet implemented"), std::string::npos) << result.stderrText;
}

// --- TS-08: metadata actually fetched via --scrap-metadata ----------------------

TEST_F(CliE2ETest, MetadataFetch)
{
    // The --scrap-metadata and --help responses are deliberately disjoint
    // (unlike TS-02's dummy, where --help's text is a superstring of
    // --scrap-metadata's): if metadata fetching silently fell back to
    // --help instead of using --scrap-metadata, this dummy would still make
    // "greet" appear in the output, but the --scrap-metadata-only marker
    // below would not.
    makeDummy("scrap-greet", R"(case "$1" in
  --scrap-metadata) echo "metadata-greets-you" ;;
  --help) echo "greet help text" ;;
  *) echo "greet called" ;;
esac)");

    auto result = runScrap({ "--help" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("greet"), std::string::npos);
    EXPECT_NE(result.stdoutText.find("metadata-greets-you"), std::string::npos);
}

/**
 * An external command answers --scrap-metadata with a description of its
 * own, so the help shows what a terminal would act on in it as text, both in
 * the listing and in the command's own help.
 */
TEST_F(CliE2ETest, ExternalDescriptionIsShownAsText)
{
    // An OSC sequence that rewrites the terminal's title.
    makeDummy("scrap-greet", R"(case "$1" in
  --scrap-metadata) printf 'greets you \033]0;pwn\007\n' ;;
esac)");

    auto listing = runScrap({ "--help" }, {}, _root);
    auto own = runScrap({ "help", "greet" }, {}, _root);

    ASSERT_TRUE(listing.exitedNormally);
    EXPECT_EQ(listing.exitCode, 0);
    EXPECT_NE(listing.stdoutText.find("greets you \\x1B]0;pwn\\x07\n"), std::string::npos) << listing.stdoutText;
    EXPECT_EQ(listing.stdoutText.find('\x1b'), std::string::npos);
    ASSERT_TRUE(own.exitedNormally);
    EXPECT_EQ(own.exitCode, 0);
    EXPECT_EQ(own.stdoutText, "USAGE: scrap greet\n\ngreets you \\x1B]0;pwn\\x07\n");
}

// --- TS-09: all version forms agree ----------------------------------------------

TEST_F(CliE2ETest, VersionForms)
{
    std::vector<std::string> banners;

    for (const auto& args :
         { std::vector<std::string>{ "version" }, std::vector<std::string>{ "--version" }, std::vector<std::string>{ "-V" } }) {
        auto result = runScrap(args, {}, _root);

        ASSERT_TRUE(result.exitedNormally);
        EXPECT_EQ(result.exitCode, 0);

        const std::string banner = trim(result.stdoutText);
        EXPECT_TRUE(isVersionBanner(banner)) << "banner: " << banner;
        banners.push_back(banner);
    }

    ASSERT_EQ(banners.size(), 3U);
    EXPECT_EQ(banners[0], banners[1]);
    EXPECT_EQ(banners[1], banners[2]);
}

// --- build: locating the project and reading its manifest ----------------------

TEST_F(CliE2ETest, BuildOutsideAProject)
{
    if (insideAProject(std::filesystem::canonical(_root))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    const std::string searched = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: could not find scrap.toml in '" + searched + "' or any parent directory\n"), std::string::npos)
        << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsAManifestErrorWithItsLocation)
{
    writeFile("scrap.toml", "[package]\nversion = \"0.1.0\"\n");
    const std::string manifest = (std::filesystem::canonical(_root) / "scrap.toml").string();

    auto result = runScrap({ "build" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find(manifest + ":1:1: error: package.name: required key is missing\n"), std::string::npos)
        << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

/**
 * The manifest belongs to the project being built, and the parser quotes
 * the bytes it read, so neither a key it names nor a value it could not read
 * reaches the terminal as bytes.
 */
TEST_F(CliE2ETest, BuildReportsAManifestErrorAsText)
{
    const std::string manifest = (std::filesystem::canonical(_root) / "scrap.toml").string();
    const std::vector<std::pair<std::string, std::string>> cases{
        { "\"k\\u001b]0;t\\u0007\" = 1", ": error: package.k\\x1B]0;t\\x07: unknown key; expected one of name version std\n" },
        { "x = tr\x1b", ": error: Error while parsing boolean: expected 'true', saw 'tr\\x1B'\n" },
    };
    for (const auto& [line, shown] : cases) {
        writeFile("scrap.toml", std::string{ ValidManifest } + line + "\n");

        auto result = runScrap({ "build" }, {}, _root);

        ASSERT_TRUE(result.exitedNormally) << line;
        EXPECT_EQ(result.exitCode, 1) << line;
        EXPECT_NE(result.stderrText.find(shown), std::string::npos) << result.stderrText;
        EXPECT_EQ(result.stderrText.find('\x1b'), std::string::npos) << result.stderrText;
        EXPECT_EQ(result.stderrText.rfind(manifest, 0), 0U) << result.stderrText;
    }
}

TEST_F(CliE2ETest, BuildFindsTheProjectAboveTheWorkingDirectory)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    std::filesystem::create_directories(_root / "src" / "detail");

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root / "src" / "detail");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("Finished debug build\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildTakesAPathRelativeToTheWorkingDirectory)
{
    // Run from outside any project, so success can only come from the path.
    if (insideAProject(std::filesystem::canonical(_root))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    writeFile("app/scrap.toml", ValidManifest);
    writeFile("app/src/main.cpp", MainSource);
    std::filesystem::create_directories(_root / "elsewhere");

    auto result = runScrap({ "build", "../app" }, { dummyCompiler() }, _root / "elsewhere");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("Finished debug build\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildRejectsAPathThatIsNotADirectory)
{
    // Inside a project, so a search from the missing path would have found one.
    writeFile("scrap.toml", ValidManifest);
    const std::string missing = (std::filesystem::canonical(_root) / "missing").string();

    auto result = runScrap({ "build", "missing" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: '" + missing + "' is not a directory\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsAPathItCannotAccess)
{
    writeFile("scrap.toml", ValidManifest);
    std::filesystem::create_symlink("loop-b", _root / "loop-a");
    std::filesystem::create_symlink("loop-a", _root / "loop-b");
    const std::string loop = (std::filesystem::canonical(_root) / "loop-a").string();

    auto result = runScrap({ "build", "loop-a" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: cannot access '" + loop + "': "), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: check the permissions of the path\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildRejectsAnEmptyPath)
{
    // Inside a project, so reading the empty path as the working directory would succeed.
    writeFile("scrap.toml", ValidManifest);

    auto result = runScrap({ "build", "" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: the path argument is empty\n"), std::string::npos) << result.stderrText;
}

// --- build: deciding what to build ---------------------------------------------

TEST_F(CliE2ETest, BuildReportsAProjectWithNothingToBuild)
{
    writeFile("scrap.toml", ValidManifest);
    const std::string project = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: no target to build in '" + project + "'\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildAcceptsAManifestThatDeclaresNothingToBuild)
{
    // An empty declaration is a statement, not an omission, so it is not an
    // error; a project that builds nothing needs no compiler to say so.
    writeFile("scrap.toml", ManifestWithoutTargets);
    std::filesystem::create_directories(_root / "empty");

    auto result = runScrap({ "build" }, { "PATH=" + (_root / "empty").string() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText.find("Using the system compiler"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Finished debug build\n"), std::string::npos) << result.stderrText;
    EXPECT_EQ(readFile("build/debug/compile_commands.json"), "[]\n");
}

TEST_F(CliE2ETest, BuildReportsASourceDirectoryItCannotRead)
{
    // The sources are read before the compiler is looked for, so nothing
    // reaches standard output: a problem in the project comes first.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    writeFile("src/locked/hidden.cpp", MainSource);
    const std::filesystem::path locked = _root / "src" / "locked";
    const std::string lockedPath = (std::filesystem::canonical(_root) / "src" / "locked").string();

    std::error_code ec;
    std::filesystem::permissions(locked, std::filesystem::perms::none, ec);
    ASSERT_FALSE(ec) << ec.message();
    // A user who reads the directory anyway, root among them, cannot observe
    // the failure; the fixture is restored before skipping so it can be removed.
    const std::filesystem::directory_iterator probe(locked, ec);
    if (! ec) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);
        GTEST_SKIP() << "this user reads a directory with no permissions";
    }

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root);

    std::filesystem::permissions(locked, std::filesystem::perms::owner_all, ec);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: cannot read '" + lockedPath + "': "), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: check the permissions of the path\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildNamesWhatItLeavesOutAndGoesOn)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    writeFile("src/Helper.CPP", MainSource);

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_NE(result.stderrText.find("warning: leaving out 'src/Helper.CPP', whose extension is not .cpp, .cc or .cxx in lower case\n"),
              std::string::npos)
        << result.stderrText;
    EXPECT_EQ(result.stderrText.find("Compiling app (src/Helper.CPP)"), std::string::npos) << result.stderrText;
}

// --- build: choosing the compiler ----------------------------------------------

TEST_F(CliE2ETest, BuildReportsTheCompilerTheVariableNames)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    makeDummy("my-compiler", "exit 0");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "my-compiler").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("Using the system compiler '" + compiler + "' (from CXX)\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsThatNoCompilerIsAvailable)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    std::filesystem::create_directories(_root / "empty");

    auto result = runScrap({ "build" }, { "PATH=" + (_root / "empty").string() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: no C++ compiler found\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsACompilerRequestItCannotRun)
{
    // CXX is the path of one program, so a value carrying a launcher names no
    // file, and the request is reported rather than passed over.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);

    auto result = runScrap({ "build" }, { "CXX=ccache g++" }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: CXX names 'ccache g++', which cannot be run\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildDoesNotReadItsOwnDirectoryForTheSystemCompiler)
{
    // A compiler in scrap's own directory is not one the system provides, so
    // the search must not find it there.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    makeDummy("c++", "exit 0");
    std::filesystem::create_directories(_root / "empty");

    auto result = runScrap({ "build" }, { "PATH=" + (_root / "empty").string() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: no C++ compiler found\n"), std::string::npos) << result.stderrText;
}

// --- build: the compilation database -------------------------------------------

TEST_F(CliE2ETest, BuildWritesTheCommandForEachSource)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    writeFile("src/util.cpp", MainSource);
    const std::string compilerOverride = dummyCompiler();
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "dummy-c++").string();
    const std::string project = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build" }, { compilerOverride }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stderrText.find("Compiling app (src/main.cpp)\n"), std::string::npos) << result.stderrText;
    EXPECT_EQ(readFile("build/debug/compile_commands.json"),
              "[\n"
              "  {\n"
              "    \"directory\": \""
                  + project
                  + "\",\n"
                    "    \"file\": \"src/main.cpp\",\n"
                    "    \"arguments\": [\""
                  + compiler
                  + "\", \"-std=c++23\", \"-g\", \"-O0\", \"-Wall\", \"-Wextra\", \"-Wpedantic\", \"-I\", \"include\", "
                    "\"-c\", \"src/main.cpp\", \"-o\", "
                    "\"build/debug/obj/app/src/main.cpp.o\"],\n"
                    "    \"output\": \"build/debug/obj/app/src/main.cpp.o\"\n"
                    "  },\n"
                    "  {\n"
                    "    \"directory\": \""
                  + project
                  + "\",\n"
                    "    \"file\": \"src/util.cpp\",\n"
                    "    \"arguments\": [\""
                  + compiler
                  + "\", \"-std=c++23\", \"-g\", \"-O0\", \"-Wall\", \"-Wextra\", \"-Wpedantic\", \"-I\", \"include\", "
                    "\"-c\", \"src/util.cpp\", \"-o\", "
                    "\"build/debug/obj/app/src/util.cpp.o\"],\n"
                    "    \"output\": \"build/debug/obj/app/src/util.cpp.o\"\n"
                    "  }\n"
                    "]\n");
}

TEST_F(CliE2ETest, BuildWritesTheReleaseCommandsApartFromTheDebugOnes)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    const std::string compilerOverride = dummyCompiler();
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "dummy-c++").string();
    const std::string project = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build", "--release" }, { compilerOverride }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Linking app (build/release/bin/app)\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Finished release build\n"), std::string::npos) << result.stderrText;
    EXPECT_EQ(readFile("build/release/compile_commands.json"),
              "[\n"
              "  {\n"
              "    \"directory\": \""
                  + project
                  + "\",\n"
                    "    \"file\": \"src/main.cpp\",\n"
                    "    \"arguments\": [\""
                  + compiler
                  + "\", \"-std=c++23\", \"-O3\", \"-DNDEBUG\", \"-Wall\", \"-Wextra\", \"-Wpedantic\", \"-I\", \"include\", "
                    "\"-c\", \"src/main.cpp\", \"-o\", "
                    "\"build/release/obj/app/src/main.cpp.o\"],\n"
                    "    \"output\": \"build/release/obj/app/src/main.cpp.o\"\n"
                    "  }\n"
                    "]\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug"));
}

TEST_F(CliE2ETest, BuildForReleaseEmptiesOnlyTheReleaseDatabase)
{
    writeFile("scrap.toml", ManifestWithoutTargets);
    writeFile("build/debug/compile_commands.json", "kept\n");
    std::filesystem::create_directories(_root / "empty");

    auto result = runScrap({ "build", "--release" }, { "PATH=" + (_root / "empty").string() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stderrText, "    Finished release build\n");
    EXPECT_EQ(readFile("build/release/compile_commands.json"), "[]\n");
    EXPECT_EQ(readFile("build/debug/compile_commands.json"), "kept\n");
}

TEST_F(CliE2ETest, NewProjectBuildsWithACompilationDatabase)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root / "work" / "hello");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    const std::string database = readFile("work/hello/build/debug/compile_commands.json");
    EXPECT_NE(database.find("\"file\": \"src/main.cpp\""), std::string::npos) << database;
    EXPECT_NE(database.find("\"-std=c++23\""), std::string::npos) << database;
    EXPECT_NE(database.find("\"output\": \"build/debug/obj/hello/src/main.cpp.o\""), std::string::npos) << database;
}

TEST_F(CliE2ETest, BuildEmptiesTheDatabaseOfAProjectThatNowBuildsNothing)
{
    // Commands left from an earlier build would keep an editor compiling
    // sources the project no longer builds.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    const std::string compilerOverride = dummyCompiler();
    auto first = runScrap({ "build" }, { compilerOverride }, _root);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;
    ASSERT_NE(readFile("build/debug/compile_commands.json"), "[]\n");

    writeFile("scrap.toml", ManifestWithoutTargets);
    auto result = runScrap({ "build" }, { compilerOverride }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_EQ(readFile("build/debug/compile_commands.json"), "[]\n");
}

TEST_F(CliE2ETest, BuildReportsABuildDirectoryItCannotCreate)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    writeFile("build", "a file where the build directory belongs\n");
    const std::string buildDirectory = (std::filesystem::canonical(_root) / "build" / "debug").string();

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: cannot create '" + buildDirectory + "': "), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: check what is already at that path\n"), std::string::npos) << result.stderrText;
}

// --- build: compiling and linking ----------------------------------------------

TEST_F(CliE2ETest, BuildReportsASourceItCannotCompile)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    makeDummy("failing-c++", "printf 'src/main.cpp:1:1: error: boom\\n' >&2\nexit 1");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "failing-c++").string();
    const std::string source = (std::filesystem::canonical(_root) / "src" / "main.cpp").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("src/main.cpp:1:1: error: boom\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("error: failed to compile '" + source + "' for 'app'\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: fix the errors reported above and run the command again\n"), std::string::npos)
        << result.stderrText;
    EXPECT_NE(readFile("build/debug/compile_commands.json"), "");
}

TEST_F(CliE2ETest, BuildLinksNothingOnceASourceFails)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/a.cpp", MainSource);
    writeFile("src/main.cpp", MainSource);
    makeDummy("picky-c++",
              "printf '%s\\n' \"$*\" >> calls.log\n"
              "case \"$*\" in *src/a.cpp*) printf 'no\\n' >&2; exit 1;; esac\n"
              "exit 0");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "picky-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    const std::string calls = readFile("calls.log");
    EXPECT_NE(calls.find("-c src/a.cpp"), std::string::npos) << calls;
    EXPECT_EQ(calls.find("-o build/debug/bin/app"), std::string::npos) << calls;
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "bin" / "app"));
}

TEST_F(CliE2ETest, BuildCompilesSourcesAtTheSameTime)
{
    if (processorsForTheBuild() < 2) {
        GTEST_SKIP() << "a build on one processor compiles one source at a time";
    }
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/a.cpp", MainSource);
    writeFile("src/main.cpp", MainSource);
    makeMeetingPoint();
    // Each compilation opens the meeting point and waits there for the other,
    // so the build ends only when both compile at once.
    makeDummy("meeting-c++",
              "case \"$*\" in\n"
              "  *'-c src/a.cpp'*) read -r line < meeting;;\n"
              "  *'-c src/main.cpp'*) printf 'here\\n' > meeting;;\n"
              "esac\n"
              "exit 0");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "meeting-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);
    releaseMeetingPoint();

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsEverySourceThatFailedAfterWhatTheCompilerWrote)
{
    if (processorsForTheBuild() < 2) {
        GTEST_SKIP() << "a build on one processor stops at the first source that fails";
    }
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/a.cpp", MainSource);
    writeFile("src/main.cpp", MainSource);
    makeMeetingPoint();
    // Both compilations are running when the first fails, so the second is
    // waited for and fails as well.
    makeDummy("meeting-c++",
              "case \"$*\" in\n"
              "  *'-c src/a.cpp'*) read -r line < meeting; printf 'src/a.cpp: error: a\\n' >&2; exit 1;;\n"
              "  *'-c src/main.cpp'*) printf 'here\\n' > meeting; printf 'src/main.cpp: error: main\\n' >&2; exit 1;;\n"
              "esac\n"
              "exit 0");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "meeting-c++").string();
    const std::string sources = (std::filesystem::canonical(_root) / "src").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);
    releaseMeetingPoint();

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    const std::string& text = result.stderrText;
    const std::size_t firstError = std::min(text.find("error: failed to compile '" + sources + "/a.cpp' for 'app'\n"),
                                            text.find("error: failed to compile '" + sources + "/main.cpp' for 'app'\n"));
    ASSERT_NE(firstError, std::string::npos) << text;
    EXPECT_NE(text.find("error: failed to compile '" + sources + "/a.cpp' for 'app'\n"), std::string::npos) << text;
    EXPECT_NE(text.find("error: failed to compile '" + sources + "/main.cpp' for 'app'\n"), std::string::npos) << text;
    EXPECT_LT(text.find("src/a.cpp: error: a\n"), firstError) << text;
    EXPECT_LT(text.find("src/main.cpp: error: main\n"), firstError) << text;
    EXPECT_TRUE(text.ends_with("'\nhint: fix the errors reported above and run the command again\n")) << text;
}

TEST_F(CliE2ETest, BuildReadsASourceNamedLikeAFileOfOptionsAsASource)
{
    // An argument starting with '@' names a file the compiler reads options
    // from, so a source named that way would decide the command it is built
    // with.
    writeFile("scrap.toml",
              "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n[[bin]]\nname = \"app\"\nsrc = "
              "\"@options.cpp\"\n");
    writeFile("options.cpp", "-DINJECTED=1\n");
    makeDummy("echoing-c++", "printf '%s\\n' \"$*\" >> calls.log\nexit 0");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "echoing-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    const std::string calls = readFile("calls.log");
    EXPECT_NE(calls.find("-c ./@options.cpp"), std::string::npos) << calls;
    EXPECT_EQ(calls.find(" @options.cpp"), std::string::npos) << calls;
}

TEST_F(CliE2ETest, BuildWritesWhatTheCompilerSaysWithoutLettingItDriveTheTerminal)
{
    // A diagnostic quotes the source it read, which an untrusted project
    // decides the contents of.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    makeDummy("shouting-c++", "printf 'title\\033]0;pwned\\007 and \\033[2J\\n' >&2\nexit 1");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "shouting-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("title\\x1B]0;pwned"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find('\033'), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildLeavesColorOutOfOutputThatIsNotATerminal)
{
    // The compiler is asked for colour, since a build usually runs in a
    // terminal; captured output holds the diagnostic and not the sequences.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    makeDummy("colorful-c++", "printf '\\033[01;31mred\\033[m\\033[K\\n' >&2\nexit 1");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "colorful-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("red\n"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find('\033'), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsMoreThanOneLibrary)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n[[lib]]\nname = \"a\"\n\n[[lib]]\nname = \"b\"\n");
    writeFile("src/core.cpp", "int answer() { return 42; }\n");

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: only one [[lib]] is supported per project\n"
              "hint: build the other libraries as projects of their own\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, BuildReportsALibraryWithoutSources)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n[[lib]]\nname = \"core\"\n");

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: the library 'core' has no sources\n"
              "hint: add sources under src/, or name one with src\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, BuildReportsAStandardTheCompilerCannotBuild)
{
    // The compiler answers as gcc 13, which has no C++26 to build against.
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\nstd = \"26\"\n");
    writeFile("src/main.cpp", MainSource);
    makeDummy("gcc13-c++",
              "case \"$*\" in *-dM*) printf '#define __GNUC__ 13\\n#define __GNUC_MINOR__ 3\\n';; esac\n"
              "exit 0");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "gcc13-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: '" + compiler + "' does not support C++26\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: use a newer compiler, or set std in scrap.toml to an older standard\n"), std::string::npos)
        << result.stderrText;
    EXPECT_NE(readFile("build/debug/compile_commands.json").find("\"-std=c++26\""), std::string::npos);
}

// --- build: with the compiler the platform provides ----------------------------

TEST_F(CliE2ETest, BuildsAndRunsAProjectWithTheRealCompiler)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;
    const std::filesystem::path project = _root / "work" / "hello";

    auto result = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Compiling hello (src/main.cpp)\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Finished debug build\n"), std::string::npos) << result.stderrText;
    ASSERT_TRUE(std::filesystem::is_regular_file(project / "build" / "debug" / "bin" / "hello"));

    auto ran = runProgram({ (project / "build" / "debug" / "bin" / "hello").string() }, {}, project);

    ASSERT_TRUE(ran.exitedNormally);
    EXPECT_EQ(ran.exitCode, 0);
    EXPECT_EQ(ran.stdoutText, "Hello, world!\n");
}

TEST_F(CliE2ETest, BuildListsWhatTheRealCompilerReadBesideTheObject)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;

    auto result = runScrap({ "build" }, { realCompiler() }, _root / "work" / "hello", BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    const std::string listed = readFile("work/hello/build/debug/obj/hello/src/main.cpp.o.d");
    EXPECT_TRUE(listed.starts_with("build/debug/obj/hello/src/main.cpp.o: src/main.cpp ")) << listed;
}

TEST_F(CliE2ETest, BuildsANewProjectForReleaseApartFromDebug)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;
    const std::filesystem::path project = _root / "work" / "hello";

    auto result = runScrap({ "build", "--release" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Linking hello (build/release/bin/hello)\n    Finished release build\n"), std::string::npos)
        << result.stderrText;
    ASSERT_TRUE(std::filesystem::is_regular_file(project / "build" / "release" / "bin" / "hello"));
    EXPECT_FALSE(std::filesystem::exists(project / "build" / "debug"));

    auto ran = runProgram({ (project / "build" / "release" / "bin" / "hello").string() }, {}, project);

    ASSERT_TRUE(ran.exitedNormally);
    EXPECT_EQ(ran.exitCode, 0);
    EXPECT_EQ(ran.stdoutText, "Hello, world!\n");
}

TEST_F(CliE2ETest, BuildsEachProfileWithoutTouchingTheOther)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", ProfileSource);

    auto debug = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(debug.exitCode, 0) << debug.stderrText;
    auto release = runScrap({ "build", "--release" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(release.exitCode, 0) << release.stderrText;

    auto ranDebug = runProgram({ (_root / "build" / "debug" / "bin" / "app").string() }, {}, _root);
    auto ranRelease = runProgram({ (_root / "build" / "release" / "bin" / "app").string() }, {}, _root);

    EXPECT_EQ(ranDebug.stdoutText, "debug\n");
    EXPECT_EQ(ranRelease.stdoutText, "release\n");
    EXPECT_NE(readFile("build/debug/compile_commands.json").find("\"-O0\""), std::string::npos);
    EXPECT_NE(readFile("build/release/compile_commands.json").find("\"-O3\""), std::string::npos);
}

TEST_F(CliE2ETest, BuildsSeveralSourcesIntoOneExecutable)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");
    writeFile("src/answer.cpp", "int answer() { return 42; }\n");

    auto result = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "obj" / "app" / "src" / "answer.cpp.o"));

    auto ran = runProgram({ (_root / "build" / "debug" / "bin" / "app").string() }, {}, _root);

    ASSERT_TRUE(ran.exitedNormally);
    EXPECT_EQ(ran.exitCode, 0);
}

TEST_F(CliE2ETest, BuildsEveryTargetOrOnlyTheOneNamed)
{
    writeFile("scrap.toml", TwoExecutablesManifest);
    writeFile("src/shared.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");
    writeFile("src/tool.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");

    auto all = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(all.exitedNormally) << all.stderrText;
    ASSERT_EQ(all.exitCode, 0) << all.stderrText;
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "bin" / "app"));
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "bin" / "tool"));

    std::filesystem::remove_all(_root / "build");
    auto one = runScrap({ "build", "--target", "tool" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(one.exitedNormally) << one.stderrText;
    ASSERT_EQ(one.exitCode, 0) << one.stderrText;
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "bin" / "tool"));
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "bin" / "app"));
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "obj" / "app"));
    // The database still describes the target left unbuilt.
    EXPECT_NE(readFile("build/debug/compile_commands.json").find("\"output\": \"build/debug/obj/app/src/main.cpp.o\""), std::string::npos);
}

TEST_F(CliE2ETest, BuildsALibraryAndLinksItIntoTheExecutable)
{
    writeFile("scrap.toml", LibraryAndExecutableManifest);
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");

    auto result = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Archiving core (build/debug/lib/libcore.a)\n"), std::string::npos) << result.stderrText;
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "lib" / "libcore.a"));
    // The library's sources are compiled for the library alone, not again
    // for the executable that uses it.
    const std::string database = readFile("build/debug/compile_commands.json");
    EXPECT_NE(database.find("\"output\": \"build/debug/obj/core/src/core.cpp.o\""), std::string::npos) << database;
    EXPECT_EQ(database.find("build/debug/obj/app/src/core.cpp.o"), std::string::npos) << database;
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "obj" / "app" / "src" / "core.cpp.o"));

    auto ran = runProgram({ (_root / "build" / "debug" / "bin" / "app").string() }, {}, _root);

    ASSERT_TRUE(ran.exitedNormally);
    EXPECT_EQ(ran.exitCode, 0);
}

/**
 * A library's own settings reach its compilation alone, and its public ones
 * reach both it and the executable that uses it, link included. Each source
 * stops the build with #error when it sees a define it should not, and the
 * executable links only if the object its library's public link flags name
 * is passed along.
 */
TEST_F(CliE2ETest, BuildPassesALibrarysPublicSettingsAndNotItsOwnToTheExecutable)
{
    writeFile("scrap.toml",
              std::string{ LibraryAndExecutableManifest } + "defines = [\"CORE_OWN\"]\n\n"
                                                            "[lib.public]\ndefines = [\"CORE_SHARED\"]\nlink-flags = [\"extra.o\"]\n");
    writeFile("src/core.cpp",
              "#if !defined(CORE_OWN) || !defined(CORE_SHARED)\n#error the library misses a setting\n#endif\n"
              "int extra();\nint answer() { return extra(); }\n");
    writeFile("src/main.cpp",
              "#ifdef CORE_OWN\n#error a setting the library keeps to itself reached the executable\n#endif\n"
              "#ifndef CORE_SHARED\n#error a public setting of the library missed the executable\n#endif\n"
              "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");
    writeFile("extra.cpp", "int extra() { return 42; }\n");
    auto extra = runProgram({ SCRAP_TEST_CXX, "-c", "extra.cpp", "-o", "extra.o" }, {}, _root, BuildTimeout);
    ASSERT_EQ(extra.exitCode, 0) << extra.stderrText;

    auto result = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    auto ran = runProgram({ (_root / "build" / "debug" / "bin" / "app").string() }, {}, _root);
    ASSERT_TRUE(ran.exitedNormally);
    EXPECT_EQ(ran.exitCode, 0);
}

TEST_F(CliE2ETest, BuildsTheLibraryTheTargetNamedUses)
{
    writeFile("scrap.toml", LibraryAndExecutableManifest);
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");

    auto result = runScrap({ "build", "--target", "app" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "lib" / "libcore.a"));
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "bin" / "app"));
}

TEST_F(CliE2ETest, RunStartsTheExecutableBesideALibrary)
{
    // The library is not a second program to choose between.
    writeFile("scrap.toml", LibraryAndExecutableManifest);
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "#include <cstdio>\nint answer();\nint main() { std::printf(\"%d\\n\", answer()); }\n");

    auto result = runScrap({ "run" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    ASSERT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "42\n");
}

TEST_F(CliE2ETest, BuildLeavesNoObjectOfARemovedSourceInTheLibrary)
{
    writeFile("scrap.toml", LibraryAndExecutableManifest);
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/gone.cpp", "int gone() { return 1; }\n");
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");
    auto first = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;
    ASSERT_NE(readFile("build/debug/lib/libcore.a").find("gone.cpp.o"), std::string::npos);

    std::filesystem::remove(_root / "src" / "gone.cpp");
    auto second = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(second.exitCode, 0) << second.stderrText;
    // An archive names each member in its header, so the name is absent once
    // the object is.
    const std::string library = readFile("build/debug/lib/libcore.a");
    EXPECT_NE(library.find("core.cpp.o"), std::string::npos);
    EXPECT_EQ(library.find("gone.cpp.o"), std::string::npos);
}

TEST_F(CliE2ETest, BuildReportsAnArchiverTheCompilerNamesAndCannotBeFound)
{
    writeFile("scrap.toml", LibraryAndExecutableManifest);
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");
    makeDummy("named-ar-c++", std::string{ "case \"$1\" in -print-prog-name=ar) echo no-such-ar ;; *) exec " } + SCRAP_TEST_CXX + " \"$@\" ;; esac");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "named-ar-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: cannot find the archiver 'no-such-ar' that '" + compiler + "' uses\n"
                                                                                                        "hint: install it, or set CXX to another compiler\n"),
              std::string::npos)
        << result.stderrText;
    // It is reported before anything is compiled, once the database is written.
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "obj"));
    EXPECT_NE(readFile("build/debug/compile_commands.json").find("\"file\": \"src/core.cpp\""), std::string::npos);
}

TEST_F(CliE2ETest, BuildReportsACompilerThatCannotNameItsArchiver)
{
    writeFile("scrap.toml", LibraryAndExecutableManifest);
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "int answer();\nint main() { return answer() == 42 ? 0 : 1; }\n");
    makeDummy("mute-c++", std::string{ "case \"$1\" in -print-prog-name=ar) exit 1 ;; *) exec " } + SCRAP_TEST_CXX + " \"$@\" ;; esac");
    const std::string compiler = (std::filesystem::canonical(_root) / "bin" / "mute-c++").string();

    auto result = runScrap({ "build" }, { "CXX=" + compiler }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: cannot ask '" + compiler + "' which archiver it uses\n"
                                                                        "hint: set CXX to a compiler that answers -print-prog-name=ar\n"),
              std::string::npos)
        << result.stderrText;
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "obj"));
}

TEST_F(CliE2ETest, BuildReportsATargetNameTheProjectLacks)
{
    writeFile("scrap.toml", TwoExecutablesManifest);
    writeFile("src/main.cpp", MainSource);
    writeFile("src/tool.cpp", MainSource);
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build", "--target", "tol" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText,
              "error: no target named 'tol' in '" + root + "': app, tool\n"
                                                           "hint: pass one of the targets listed, or omit --target to build them all\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, BuildReportsAnEmptyTargetName)
{
    // An empty value, as an unset shell variable leaves, builds nothing
    // rather than everything.
    writeFile("scrap.toml", TwoExecutablesManifest);
    writeFile("src/main.cpp", MainSource);
    writeFile("src/tool.cpp", MainSource);
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build", "--target", "" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: no target named '' in '" + root + "': app, tool\n"
                                                        "hint: pass one of the targets listed, or omit --target to build them all\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, BuildKeepsTheDatabaseWhenTheRealCompilerReportsAnError)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "int main() { return 0 }\n");
    const std::string source = (std::filesystem::canonical(_root) / "src" / "main.cpp").string();

    auto result = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 1) << result.stderrText;
    EXPECT_NE(result.stderrText.find("src/main.cpp:1:"), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("error: failed to compile '" + source + "' for 'app'\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(readFile("build/debug/compile_commands.json").find("\"file\": \"src/main.cpp\""), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "bin" / "app"));
}

// --- build: building only what changed -----------------------------------------

/**
 * How many times @p text holds @p part.
 */
[[nodiscard]] static std::size_t occurrences(const std::string& text, std::string_view part)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) {
        ++count;
    }
    return count;
}

TEST_F(CliE2ETest, BuildCompilesAndLinksNothingWhenNothingChanged)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;
    const std::filesystem::path project = _root / "work" / "hello";
    auto first = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;
    const auto built = std::filesystem::last_write_time(project / "build" / "debug" / "bin" / "hello");

    auto again = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_TRUE(again.exitedNormally) << again.stderrText;
    EXPECT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(again.stderrText.find("Compiling"), std::string::npos) << again.stderrText;
    EXPECT_EQ(again.stderrText.find("Linking"), std::string::npos) << again.stderrText;
    EXPECT_NE(again.stderrText.find("Finished debug build\n"), std::string::npos) << again.stderrText;
    EXPECT_EQ(std::filesystem::last_write_time(project / "build" / "debug" / "bin" / "hello"), built);

    auto ran = runScrap({ "run" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_TRUE(ran.exitedNormally) << ran.stderrText;
    EXPECT_EQ(ran.exitCode, 0) << ran.stderrText;
    EXPECT_EQ(ran.stderrText.find("Compiling"), std::string::npos) << ran.stderrText;
    EXPECT_EQ(ran.stderrText.find("Linking"), std::string::npos) << ran.stderrText;
    EXPECT_EQ(ran.stdoutText, "Hello, world!\n");
}

TEST_F(CliE2ETest, BuildCompilesAgainOnlyTheSourcesAChangeReaches)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n");
    writeFile("include/greeting.h", "inline int greeting() { return 1; }\n");
    writeFile("src/main.cpp", "#include \"greeting.h\"\nint other();\nint main() { return greeting() + other() == 3 ? 0 : 1; }\n");
    writeFile("src/other.cpp", "int other() { return 2; }\n");
    auto first = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    writeFile("src/other.cpp", "int other() { return 2 + 0; }\n");
    auto afterSource = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(afterSource.exitCode, 0) << afterSource.stderrText;
    EXPECT_EQ(occurrences(afterSource.stderrText, "Compiling"), 1) << afterSource.stderrText;
    EXPECT_NE(afterSource.stderrText.find("Compiling app (src/other.cpp)\n"), std::string::npos) << afterSource.stderrText;
    EXPECT_NE(afterSource.stderrText.find("Linking app (build/debug/bin/app)\n"), std::string::npos) << afterSource.stderrText;

    writeFile("include/greeting.h", "inline int greeting() { return 1 + 0; }\n");
    auto afterHeader = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(afterHeader.exitCode, 0) << afterHeader.stderrText;
    EXPECT_EQ(occurrences(afterHeader.stderrText, "Compiling"), 1) << afterHeader.stderrText;
    EXPECT_NE(afterHeader.stderrText.find("Compiling app (src/main.cpp)\n"), std::string::npos) << afterHeader.stderrText;
}

TEST_F(CliE2ETest, BuildCompilesTheExecutableAgainWhenItsLibrarysPublicHeaderChanges)
{
    writeFile("scrap.toml", std::string{ LibraryAndExecutableManifest } + "\n[lib.public]\ninclude-dirs = [\"core-include\"]\n");
    writeFile("core-include/core.h", "int answer();\ninline int offset() { return 0; }\n");
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    writeFile("src/main.cpp", "#include \"core.h\"\nint main() { return answer() + offset() == 42 ? 0 : 1; }\n");
    auto first = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    writeFile("core-include/core.h", "int answer();\ninline int offset() { return 0 + 0; }\n");
    auto again = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(occurrences(again.stderrText, "Compiling"), 1) << again.stderrText;
    EXPECT_NE(again.stderrText.find("Compiling app (src/main.cpp)\n"), std::string::npos) << again.stderrText;
    EXPECT_EQ(again.stderrText.find("Archiving"), std::string::npos) << again.stderrText;
}

TEST_F(CliE2ETest, BuildCompilesAgainAHeaderAppearingWhereItIsFoundFirst)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n");
    writeFile("include/value.h", "inline int value() { return 1; }\n");
    writeFile("src/main.cpp", "#include \"value.h\"\nint main() { return value(); }\n");
    auto first = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    writeFile("src/value.h", "inline int value() { return 0; }\n");
    auto again = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_NE(again.stderrText.find("Compiling app (src/main.cpp)\n"), std::string::npos) << again.stderrText;
    auto ran = runProgram({ (_root / "build" / "debug" / "bin" / "app").string() }, {}, _root);
    EXPECT_EQ(ran.exitCode, 0);
}

TEST_F(CliE2ETest, BuildCompilesAgainAHeaderAppearingBesideTheHeaderThatNamesIt)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n");
    writeFile("include/lib/a.h", "#include \"b.h\"\ninline int a() { return b(); }\n");
    writeFile("include/b.h", "inline int b() { return 1; }\n");
    writeFile("src/main.cpp", "#include \"lib/a.h\"\nint main() { return a(); }\n");
    auto first = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    writeFile("include/lib/b.h", "inline int b() { return 0; }\n");
    auto again = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_NE(again.stderrText.find("Compiling app (src/main.cpp)\n"), std::string::npos) << again.stderrText;
    auto ran = runProgram({ (_root / "build" / "debug" / "bin" / "app").string() }, {}, _root);
    EXPECT_EQ(ran.exitCode, 0);
}

TEST_F(CliE2ETest, BuildCompilesNothingForAHeaderAppearingWhereNothingLooksForIt)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n");
    writeFile("include/lib/a.h", "#include \"b.h\"\ninline int a() { return b(); }\n");
    writeFile("include/b.h", "inline int b() { return 0; }\n");
    writeFile("src/main.cpp", "#include \"lib/a.h\"\nint main() { return a(); }\n");
    auto first = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    writeFile("src/b.h", "inline int b() { return 1; }\n");
    auto again = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(occurrences(again.stderrText, "Compiling"), 0) << again.stderrText;
}

TEST_F(CliE2ETest, BuildCompilesAgainWhenTheCompileFlagsChange)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;
    const std::filesystem::path project = _root / "work" / "hello";
    auto first = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    writeFile("work/hello/scrap.toml",
              readFile("work/hello/scrap.toml") + "\n[[bin]]\nname = \"hello\"\nsrc = \"src/main.cpp\"\ncompile-flags = [\"-DCHANGED_FLAG\"]\n");
    auto again = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_NE(again.stderrText.find("Compiling hello (src/main.cpp)\n"), std::string::npos) << again.stderrText;
    EXPECT_NE(readFile("work/hello/build/debug/compile_commands.json").find("\"-DCHANGED_FLAG\""), std::string::npos);
}

TEST_F(CliE2ETest, BuildCompilesEverythingAgainWithoutItsRecords)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;
    const std::filesystem::path project = _root / "work" / "hello";
    auto first = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);
    ASSERT_EQ(first.exitCode, 0) << first.stderrText;

    std::filesystem::remove_all(project / "build" / "debug" / "records");
    auto removed = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_EQ(removed.exitCode, 0) << removed.stderrText;
    EXPECT_NE(removed.stderrText.find("Compiling hello (src/main.cpp)\n"), std::string::npos) << removed.stderrText;
    EXPECT_NE(removed.stderrText.find("Linking hello (build/debug/bin/hello)\n"), std::string::npos) << removed.stderrText;

    writeFile("work/hello/build/debug/records/obj/hello/src/main.cpp.o", "broken\n");
    writeFile("work/hello/build/debug/records/bin/hello", "broken\n");
    auto broken = runScrap({ "build" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_EQ(broken.exitCode, 0) << broken.stderrText;
    EXPECT_NE(broken.stderrText.find("Compiling hello (src/main.cpp)\n"), std::string::npos) << broken.stderrText;
    EXPECT_NE(broken.stderrText.find("Linking hello (build/debug/bin/hello)\n"), std::string::npos) << broken.stderrText;
}

// --- run: building and starting the executable ---------------------------------

TEST_F(CliE2ETest, RunBuildsAndRunsANewProject)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;
    const std::filesystem::path project = _root / "work" / "hello";

    auto result = runScrap({ "run" }, { realCompiler() }, project, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "Hello, world!\n");
    EXPECT_NE(result.stderrText.find("Finished debug build\n     Running hello (build/debug/bin/hello)\n"), std::string::npos)
        << result.stderrText;
}

TEST_F(CliE2ETest, RunBuildsAndRunsTheReleaseBuild)
{
    // A debug build already in place is not what runs.
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", ProfileSource);
    auto debug = runScrap({ "build" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(debug.exitCode, 0) << debug.stderrText;

    auto result = runScrap({ "run", "--release" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "release\n");
    EXPECT_NE(result.stderrText.find("Finished release build\n     Running app (build/release/bin/app)\n"), std::string::npos)
        << result.stderrText;
}

TEST_F(CliE2ETest, HelpForRunDescribesTheReleaseProfile)
{
    auto result = runScrap({ "help", "run" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("--release"), std::string::npos) << result.stdoutText;
    EXPECT_NE(result.stdoutText.find("Build and run with the release profile\n"), std::string::npos) << result.stdoutText;
}

TEST_F(CliE2ETest, RunPassesTheArgumentsAfterTheSeparator)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp",
              "#include <cstdio>\n"
              "int main(int argc, char** argv) { for (int i = 1; i < argc; ++i) std::printf(\"[%s]\", argv[i]); return 0; }\n");

    auto result = runScrap({ "run", "--", "alpha", "beta", "", "--help" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "[alpha][beta][][--help]");
}

TEST_F(CliE2ETest, RunReturnsTheExitCodeOfTheProgram)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "#include <cstdlib>\nint main(int, char** argv) { return std::atoi(argv[1]); }\n");

    for (const char* code : { "0", "3", "42" }) {
        auto result = runScrap({ "run", "--", code }, { realCompiler() }, _root, BuildTimeout);

        ASSERT_TRUE(result.exitedNormally) << result.stderrText;
        EXPECT_EQ(result.exitCode, std::atoi(code)) << result.stderrText;
    }
}

TEST_F(CliE2ETest, RunReportsASignalAsAShellDoes)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "#include <csignal>\nint main() { std::raise(SIGTERM); return 0; }\n");

    auto result = runScrap({ "run" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 128 + SIGTERM) << result.stderrText;
}

TEST_F(CliE2ETest, RunEndsByTheInterruptThatStoppedTheProgram)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "#include <csignal>\nint main() { std::raise(SIGINT); return 0; }\n");

    auto result = runScrap({ "run" }, { realCompiler() }, _root, BuildTimeout);

    EXPECT_FALSE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.signal, SIGINT) << result.stderrText;
}

TEST_F(CliE2ETest, RunEndsByTheQuitThatStoppedTheProgram)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "#include <csignal>\nint main() { std::raise(SIGQUIT); return 0; }\n");

    auto result = runScrap({ "run" }, { realCompiler() }, _root, BuildTimeout);

    EXPECT_FALSE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.signal, SIGQUIT) << result.stderrText;
}

TEST_F(CliE2ETest, RunDoesNotStartAProgramThatFailedToBuild)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", "#include <cstdio>\nint main() { std::puts(\"built before\"); return 0; }\n");
    auto first = runScrap({ "run" }, { realCompiler() }, _root, BuildTimeout);
    ASSERT_EQ(first.stdoutText, "built before\n") << first.stderrText;
    writeFile("src/main.cpp", "int main() { return 0 }\n");

    auto result = runScrap({ "run" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 101) << result.stderrText;
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: failed to compile '"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find("Running"), std::string::npos) << result.stderrText;
    // The executable of the build before is left in place, as make leaves it.
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "debug" / "bin" / "app"));
}

TEST_F(CliE2ETest, RunStartsTheProgramInTheWorkingDirectory)
{
    // Run from outside any project, so the project can only come from the path.
    if (insideAProject(std::filesystem::canonical(_root))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    writeFile("app/scrap.toml", ValidManifest);
    writeFile("app/src/main.cpp",
              "#include <cstdio>\n#include <filesystem>\n"
              "int main() { std::fputs(std::filesystem::current_path().c_str(), stdout); return 0; }\n");
    std::filesystem::create_directories(_root / "elsewhere");

    auto result = runScrap({ "run", "../app" }, { realCompiler() }, _root / "elsewhere", BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(std::filesystem::canonical(result.stdoutText), std::filesystem::canonical(_root / "elsewhere"));
}

TEST_F(CliE2ETest, RunReportsAProjectWithNoExecutable)
{
    writeFile("scrap.toml", ManifestWithoutTargets);
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText,
              "error: no executable to run in '" + root + "'\n"
                                                          "hint: add a [[bin]] section to scrap.toml, or create src/main.cpp\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, RunReportsALibraryOnlyProject)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n[[lib]]\nname = \"core\"\n");
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText,
              "error: no executable to run in '" + root + "'\n"
                                                          "hint: add a [[bin]] section to scrap.toml, or create src/main.cpp\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, RunReportsAProjectWithNoExecutableWhateverItIsAskedFor)
{
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n[[lib]]\nname = \"core\"\n");
    writeFile("src/core.cpp", "int answer() { return 42; }\n");
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run", "--bin", "core" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: no executable to run in '" + root + "'\n"
                                                          "hint: add a [[bin]] section to scrap.toml, or create src/main.cpp\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, BuildReportsATargetNameInAProjectWithNoTargets)
{
    writeFile("scrap.toml", ManifestWithoutTargets);
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "build", "--target", "app" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: no target named 'app' in '" + root + "'\n"
                                                           "hint: omit --target, since scrap.toml declares no targets\n");
}

TEST_F(CliE2ETest, RunReportsAProjectWithMoreThanOneExecutable)
{
    writeFile("scrap.toml",
              "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n"
              "[[bin]]\nname = \"app\"\nsrc = \"src/main.cpp\"\n\n"
              "[[bin]]\nname = \"tool\"\nsrc = \"src/tool.cpp\"\n");
    writeFile("src/main.cpp", MainSource);
    writeFile("src/tool.cpp", MainSource);
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText,
              "error: more than one executable to run in '" + root + "': app, tool\n"
                                                                     "hint: pass --bin with one of them\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, RunStartsTheExecutableNamedByBin)
{
    writeFile("scrap.toml", TwoExecutablesManifest);
    writeFile("src/main.cpp", "#include <cstdio>\nint main() { std::puts(\"app\"); return 0; }\n");
    writeFile("src/tool.cpp", "#include <cstdio>\nint main() { std::puts(\"tool\"); return 0; }\n");

    auto result = runScrap({ "run", "--bin", "tool" }, { realCompiler() }, _root, BuildTimeout);

    ASSERT_TRUE(result.exitedNormally) << result.stderrText;
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "tool\n");
    EXPECT_NE(result.stderrText.find("     Running tool (build/debug/bin/tool)\n"), std::string::npos) << result.stderrText;
    // Only what the executable needs is built.
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "bin" / "app"));
}

TEST_F(CliE2ETest, RunReportsAnExecutableNameTheProjectLacks)
{
    writeFile("scrap.toml",
              "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n"
              "[[bin]]\nname = \"app\"\nsrc = \"src/main.cpp\"\n\n"
              "[[lib]]\nname = \"mylib\"\nsrc = \"src/lib.cpp\"\n");
    writeFile("src/main.cpp", MainSource);
    writeFile("src/lib.cpp", "int answer() { return 42; }\n");
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run", "--bin", "mylib" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText,
              "error: no executable named 'mylib' in '" + root + "': app\n"
                                                                 "hint: pass one of the executables listed\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, RunReportsAnEmptyExecutableName)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    const std::string root = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run", "--bin", "" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: no executable named '' in '" + root + "': app\n"
                                                            "hint: pass one of the executables listed\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, RunOutsideAProject)
{
    if (insideAProject(std::filesystem::canonical(_root))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    const std::string searched = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "run" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: could not find scrap.toml in '" + searched + "' or any parent directory\n"), std::string::npos)
        << result.stderrText;
}

TEST_F(CliE2ETest, RunRejectsAnEmptyPath)
{
    writeFile("scrap.toml", ValidManifest);

    auto result = runScrap({ "run", "" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: the path argument is empty\n"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildRejectsArgumentsAfterTheSeparator)
{
    writeFile("scrap.toml", ValidManifest);

    auto result = runScrap({ "build", "--", "alpha" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_NE(result.stderrText.find("The following argument was not expected: alpha\n"), std::string::npos) << result.stderrText;
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

// --- clean: removing the build directory ---------------------------------------

TEST_F(CliE2ETest, CleanRemovesWhatABuildWrote)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("src/main.cpp", MainSource);
    auto built = runScrap({ "build" }, { dummyCompiler() }, _root);
    ASSERT_EQ(built.exitCode, 0) << built.stderrText;
    ASSERT_TRUE(std::filesystem::is_directory(_root / "build" / "debug"));
    const std::string buildDirectory = (std::filesystem::canonical(_root) / "build").string();

    auto result = runScrap({ "clean" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "Removed '" + buildDirectory + "'\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "src" / "main.cpp"));
}

TEST_F(CliE2ETest, CleanRemovesWhatElseIsInTheBuildDirectory)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("build/other-tool/cache.txt", "written by another tool\n");

    auto result = runScrap({ "clean" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
}

TEST_F(CliE2ETest, CleanSucceedsWithNothingToRemove)
{
    writeFile("scrap.toml", ValidManifest);
    const std::string buildDirectory = (std::filesystem::canonical(_root) / "build").string();

    auto result = runScrap({ "clean" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "Nothing to remove at '" + buildDirectory + "'\n");
}

TEST_F(CliE2ETest, CleanTakesAPathRelativeToTheWorkingDirectory)
{
    // Run from outside any project, so success can only come from the path.
    if (insideAProject(std::filesystem::canonical(_root))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    writeFile("app/scrap.toml", ValidManifest);
    writeFile("app/build/debug/compile_commands.json", "[]\n");
    std::filesystem::create_directories(_root / "elsewhere");

    auto result = runScrap({ "clean", "../app" }, {}, _root / "elsewhere");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(std::filesystem::exists(_root / "app" / "build"));
}

TEST_F(CliE2ETest, CleanFindsTheProjectAboveTheWorkingDirectory)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("build/debug/compile_commands.json", "[]\n");
    std::filesystem::create_directories(_root / "src" / "detail");

    auto result = runScrap({ "clean" }, {}, _root / "src" / "detail");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_FALSE(std::filesystem::exists(_root / "build"));
    EXPECT_TRUE(std::filesystem::is_directory(_root / "src" / "detail"));
}

TEST_F(CliE2ETest, CleanOutsideAProject)
{
    if (insideAProject(std::filesystem::canonical(_root))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    writeFile("build/kept.txt", "not a project's\n");
    const std::string searched = std::filesystem::canonical(_root).string();

    auto result = runScrap({ "clean" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: could not find scrap.toml in '" + searched + "' or any parent directory\n"), std::string::npos)
        << result.stderrText;
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "build" / "kept.txt"));
}

TEST_F(CliE2ETest, CleanRejectsAnEmptyPath)
{
    // Inside a project, so reading the empty path as the working directory would remove it.
    writeFile("scrap.toml", ValidManifest);
    writeFile("build/debug/compile_commands.json", "[]\n");

    auto result = runScrap({ "clean", "" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: the path argument is empty\n"), std::string::npos) << result.stderrText;
    EXPECT_TRUE(std::filesystem::is_directory(_root / "build"));
}

TEST_F(CliE2ETest, CleanRemovesALinkWithoutFollowingIt)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("elsewhere/kept.txt", "kept\n");
    std::filesystem::create_directory_symlink(_root / "elsewhere", _root / "build");

    auto result = runScrap({ "clean" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    std::error_code ec;
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(_root / "build", ec)));
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "elsewhere" / "kept.txt"));
}

TEST_F(CliE2ETest, CleanLeavesAFileWhereTheBuildDirectoryBelongs)
{
    writeFile("scrap.toml", ValidManifest);
    writeFile("build", "a file of the user's\n");
    const std::string buildPath = (std::filesystem::canonical(_root) / "build").string();

    auto result = runScrap({ "clean" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText,
              "error: '" + buildPath + "' is not a directory\n"
                                       "hint: check what is already at that path\n");
    EXPECT_EQ(readFile("build"), "a file of the user's\n");
}

// --- new: creating a project ---------------------------------------------------

TEST_F(CliE2ETest, NewCreatesAProject)
{
    std::filesystem::create_directories(_root / "work");
    const std::string project = (std::filesystem::canonical(_root / "work") / "hello").string();

    auto result = runScrap({ "new", "hello" }, {}, _root / "work");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_EQ(result.stderrText, "Created project 'hello' at '" + project + "'\n");
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "work" / "hello" / "scrap.toml"));
    EXPECT_TRUE(std::filesystem::is_regular_file(_root / "work" / "hello" / "src" / "main.cpp"));
}

TEST_F(CliE2ETest, NewProjectLoadsInBuild)
{
    std::filesystem::create_directories(_root / "work");
    auto created = runScrap({ "new", "hello" }, {}, _root / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root / "work" / "hello");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
}

TEST_F(CliE2ETest, NewLeavesAnExistingDirectoryUntouched)
{
    writeFile("work/hello/marker.txt", "keep me\n");
    const std::string project = (std::filesystem::canonical(_root / "work") / "hello").string();

    auto result = runScrap({ "new", "hello" }, {}, _root / "work");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: '" + project + "' already exists\n"), std::string::npos) << result.stderrText;
    std::ifstream marker(_root / "work" / "hello" / "marker.txt", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(marker), std::istreambuf_iterator<char>()), "keep me\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "work" / "hello" / "scrap.toml"));
}

TEST_F(CliE2ETest, NewRejectsNamesItCannotUse)
{
    std::filesystem::create_directories(_root / "work");

    for (const char* name : { "", "../x", "a/b" }) {
        auto result = runScrap({ "new", name }, {}, _root / "work");

        ASSERT_TRUE(result.exitedNormally) << name;
        EXPECT_EQ(result.exitCode, 1) << name;
        EXPECT_TRUE(result.stdoutText.empty()) << name << ": " << result.stdoutText;
        EXPECT_NE(result.stderrText.find("error: "), std::string::npos) << name << ": " << result.stderrText;
        EXPECT_NE(result.stderrText.find("\nhint: use up to 64 letters, digits, '-' and '_', starting with a letter\n"), std::string::npos)
            << name << ": " << result.stderrText;
    }
    EXPECT_TRUE(std::filesystem::is_empty(_root / "work"));
    EXPECT_FALSE(std::filesystem::exists(_root / "x"));
}

/**
 * A selected toolchain is kept in config.toml for later commands; a family
 * alone is kept as its newest installed version; one that is not installed
 * is refused and leaves the default as it was; unselect removes it.
 */
TEST_F(CliE2ETest, SelectsAndUnselectsTheGlobalDefault)
{
    installToolchain("gcc", "15.1.0");
    installToolchain("gcc", "15.2.0");

    auto exact = runScrap({ "toolchain", "select", "gcc@15.1.0" }, {}, _root);
    ASSERT_TRUE(exact.exitedNormally);
    EXPECT_EQ(exact.exitCode, 0) << exact.stderrText;
    EXPECT_TRUE(exact.stdoutText.empty()) << exact.stdoutText;
    EXPECT_EQ(exact.stderrText, "Selected 'gcc@15.1.0' as the global default\n");
    EXPECT_NE(readFile("config.toml").find("toolchain = 'gcc@15.1.0'"), std::string::npos) << readFile("config.toml");

    auto family = runScrap({ "toolchain", "select", "gcc" }, {}, _root);
    EXPECT_EQ(family.exitCode, 0) << family.stderrText;
    EXPECT_EQ(family.stderrText, "Selected 'gcc@15.2.0' as the global default\n");

    auto missing = runScrap({ "toolchain", "select", "gcc@14" }, {}, _root);
    EXPECT_EQ(missing.exitCode, 1);
    EXPECT_EQ(missing.stderrText, "error: toolchain 'gcc@14' is not installed\nhint: run 'scrap toolchain install gcc@14'\n");
    EXPECT_NE(readFile("config.toml").find("toolchain = 'gcc@15.2.0'"), std::string::npos) << readFile("config.toml");

    auto unselect = runScrap({ "toolchain", "unselect" }, {}, _root);
    EXPECT_EQ(unselect.exitCode, 0) << unselect.stderrText;
    EXPECT_EQ(unselect.stderrText, "Unselected 'gcc@15.2.0'; no toolchain is selected globally\n");
    EXPECT_EQ(readFile("config.toml").find("toolchain"), std::string::npos) << readFile("config.toml");

    auto again = runScrap({ "toolchain", "unselect" }, {}, _root);
    EXPECT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(again.stderrText, "No toolchain is selected globally\n");
}

/**
 * The word system selects the compiler found on the system, installed or
 * not.
 */
TEST_F(CliE2ETest, SelectsTheSystemCompiler)
{
    auto result = runScrap({ "toolchain", "select", "system" }, {}, _root);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_EQ(result.stderrText, "Selected 'system' as the global default\n");
    EXPECT_NE(readFile("config.toml").find("toolchain = 'system'"), std::string::npos) << readFile("config.toml");
}

/**
 * Text that is not a specifier is refused with its reason, and nothing is
 * written.
 */
TEST_F(CliE2ETest, SelectRefusesTextThatIsNotASpecifier)
{
    auto result = runScrap({ "toolchain", "select", "gcc@" }, {}, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: 'gcc@' is not a toolchain specifier: it names no version after '@'\n"
              "hint: write it as <family>[@<version>], such as 'gcc@15', or as system\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "config.toml"));
}

/**
 * A config.toml that cannot be read is pointed at as a compiler points at a
 * fault, and is not written over, so what it holds is not lost.
 */
TEST_F(CliE2ETest, SelectLeavesAConfigItCannotReadAsItIs)
{
    installToolchain("gcc", "15.2.0");
    writeFile("config.toml", "[defaults]\ntoolchain = 15\n");
    const std::string config = (_root / "config.toml").string();

    for (const std::vector<std::string>& args : { std::vector<std::string>{ "toolchain", "select", "gcc" }, std::vector<std::string>{ "toolchain", "unselect" } }) {
        auto result = runScrap(args, {}, _root);

        EXPECT_EQ(result.exitCode, 1) << args.back();
        EXPECT_EQ(result.stderrText,
                  config + ":2:13: error: defaults.toolchain: must be a string\n"
                           "hint: fix the file, or remove it to clear the settings it holds\n")
            << args.back();
        EXPECT_EQ(readFile("config.toml"), "[defaults]\ntoolchain = 15\n") << args.back();
    }
}

/**
 * Without SCRAP_HOME and HOME there is nowhere to keep a default, which the
 * toolchain commands report.
 */
TEST_F(CliE2ETest, ToolchainCommandsNeedSomewhereToKeepToolchains)
{
    for (const std::vector<std::string>& args : { std::vector<std::string>{ "toolchain", "select", "system" }, std::vector<std::string>{ "toolchain", "unselect" } }) {
        auto result = runScrap(args, { "SCRAP_HOME=" }, _root);

        EXPECT_EQ(result.exitCode, 1) << args.back();
        EXPECT_EQ(result.stderrText,
                  "error: cannot tell where to keep toolchains: SCRAP_HOME and HOME are both unset\n"
                  "hint: set SCRAP_HOME to the directory to keep them in, and run the command again\n")
            << args.back();
    }
    EXPECT_FALSE(std::filesystem::exists(_root / "config.toml"));
}

/**
 * The toolchain a build uses comes from SCRAP_TOOLCHAIN, then the project's
 * [toolchain] name, then the global default, and the build says which; the
 * same installed toolchain serves another project too.
 */
TEST_F(CliE2ETest, BuildUsesTheToolchainChosenFirst)
{
    installWorkingToolchain("gcc", "15.1.0");
    installWorkingToolchain("gcc", "15.2.0");
    ASSERT_EQ(runScrap({ "toolchain", "select", "gcc@15.1.0" }, {}, _root).exitCode, 0);
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);
    const std::filesystem::path hello = _root / "hello";

    auto global = runScrap({ "build" }, {}, hello, BuildTimeout);
    EXPECT_EQ(global.exitCode, 0) << global.stderrText;
    EXPECT_NE(global.stderrText.find("Using the toolchain 'gcc@15.1.0' (from the global default)\n"), std::string::npos) << global.stderrText;
    EXPECT_TRUE(compilersUsed().ends_with("gcc@15.1.0\n")) << compilersUsed();

    writeFile("hello/scrap.toml", std::string{ ValidManifest } + "[toolchain]\nname = \"gcc@15.2.0\"\n");
    auto project = runScrap({ "build" }, {}, hello, BuildTimeout);
    EXPECT_EQ(project.exitCode, 0) << project.stderrText;
    EXPECT_NE(project.stderrText.find("Using the toolchain 'gcc@15.2.0' (from scrap.toml)\n"), std::string::npos) << project.stderrText;
    EXPECT_TRUE(compilersUsed().ends_with("gcc@15.2.0\n")) << compilersUsed();

    auto variable = runScrap({ "build" }, { "SCRAP_TOOLCHAIN=gcc@15.1.0" }, hello, BuildTimeout);
    EXPECT_EQ(variable.exitCode, 0) << variable.stderrText;
    EXPECT_NE(variable.stderrText.find("Using the toolchain 'gcc@15.1.0' (from SCRAP_TOOLCHAIN, overriding scrap.toml)\n"), std::string::npos)
        << variable.stderrText;
    EXPECT_TRUE(compilersUsed().ends_with("gcc@15.1.0\n")) << compilersUsed();

    writeFile("other/scrap.toml", ValidManifest);
    writeFile("other/src/main.cpp", MainSource);
    auto other = runScrap({ "build" }, { "SCRAP_TOOLCHAIN=gcc@15.2.0" }, _root / "other", BuildTimeout);
    EXPECT_EQ(other.exitCode, 0) << other.stderrText;
    EXPECT_NE(other.stderrText.find("Using the toolchain 'gcc@15.2.0' (from SCRAP_TOOLCHAIN)\n"), std::string::npos) << other.stderrText;
}

/**
 * An executable built with an installed toolchain is linked to search the
 * toolchain's runtime library directory at run time.
 */
TEST_F(CliE2ETest, BuildEmbedsTheRuntimeLibraryDirectoryOfTheToolchain)
{
    installWorkingToolchain("gcc", "15.2.0");
    recordRuntimeLibraryDirectory("gcc", "15.2.0", "lib64");
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);

    auto result = runScrap({ "build" }, { "SCRAP_TOOLCHAIN=gcc@15.2.0" }, _root / "hello", BuildTimeout);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    const std::string arguments = readFile("compiler-arguments");
    const std::string embedding = "-Xlinker\n-rpath\n-Xlinker\n";
    const auto at = arguments.find(embedding);
    ASSERT_NE(at, std::string::npos) << arguments;
    const auto start = at + embedding.size();
    const std::string directory = arguments.substr(start, arguments.find('\n', start) - start);
    EXPECT_TRUE(directory.ends_with("/lib64")) << directory;
    EXPECT_NE(directory.find("toolchains/gcc/15.2.0/"), std::string::npos) << directory;
    EXPECT_TRUE(std::filesystem::path{ directory }.is_absolute()) << directory;
}

/**
 * The arguments the record of an installed toolchain names are given to
 * its compiler: the compile ones to each compile, the link ones to each
 * link.
 */
TEST_F(CliE2ETest, BuildGivesTheCompilerTheArgumentsTheToolchainRecords)
{
    installWorkingToolchain("llvm", "22.1.8");
    recordToolchainArguments("llvm", "22.1.8", "-DSCRAP_RECORDED_COMPILE_ARGUMENT", "-Wl,-rpath,/scrap-recorded-link-argument");
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);

    auto result = runScrap({ "build" }, { "SCRAP_TOOLCHAIN=llvm@22.1.8" }, _root / "hello", BuildTimeout);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    const std::string arguments = readFile("compiler-arguments");
    const auto compile = arguments.find("-DSCRAP_RECORDED_COMPILE_ARGUMENT\n");
    const auto link = arguments.find("-Wl,-rpath,/scrap-recorded-link-argument\n");
    ASSERT_NE(compile, std::string::npos) << arguments;
    ASSERT_NE(link, std::string::npos) << arguments;
    EXPECT_LT(arguments.find("\n-c\n"), link) << arguments;
    EXPECT_GT(arguments.find("\n-c\n", compile), compile) << arguments;
    EXPECT_EQ(arguments.find("-DSCRAP_RECORDED_COMPILE_ARGUMENT\n", compile + 1), std::string::npos) << arguments;
}

/**
 * On macOS the compiler of an installed toolchain is pointed at the SDK
 * xcrun names, unless SDKROOT names one, which the compiler reads itself.
 * Elsewhere it is given no SDK.
 */
TEST_F(CliE2ETest, BuildPointsAnInstalledToolchainAtTheMacOsSdk)
{
    installWorkingToolchain("llvm", "22.1.8");
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);

    auto result = runScrap({ "build" }, { "SCRAP_TOOLCHAIN=llvm@22.1.8" }, _root / "hello", BuildTimeout);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    const std::string arguments = readFile("compiler-arguments");
#if defined(__APPLE__)
    const std::string option = "-isysroot\n";
    ASSERT_NE(arguments.find(option), std::string::npos) << arguments;
    const auto start = arguments.find(option) + option.size();
    const std::filesystem::path sdk = arguments.substr(start, arguments.find('\n', start) - start);
    EXPECT_TRUE(std::filesystem::is_directory(sdk)) << sdk;

    writeFile("compiler-arguments", "");
    auto named = runScrap({ "build", "--release" }, { "SCRAP_TOOLCHAIN=llvm@22.1.8", "SDKROOT=" + sdk.string() }, _root / "hello", BuildTimeout);
    EXPECT_EQ(named.exitCode, 0) << named.stderrText;
    EXPECT_NE(readFile("compiler-arguments").find("-c\n"), std::string::npos);
    EXPECT_EQ(readFile("compiler-arguments").find("-isysroot"), std::string::npos);

    // The same compiler, taken as the system's, finds the SDK itself. Its
    // commands are those of the build before, so that build is removed.
    writeFile("compiler-arguments", "");
    std::filesystem::remove_all(_root / "hello" / "build");
    const std::filesystem::path compiler = _root / "toolchains/llvm/22.1.8/darwin-aarch64/bin/g++";
    auto system = runScrap({ "build", "--release" }, { "SCRAP_TOOLCHAIN=system", "CXX=" + compiler.string() }, _root / "hello", BuildTimeout);
    EXPECT_EQ(system.exitCode, 0) << system.stderrText;
    EXPECT_NE(readFile("compiler-arguments").find("-c\n"), std::string::npos);
    EXPECT_EQ(readFile("compiler-arguments").find("-isysroot"), std::string::npos);
#else
    EXPECT_EQ(arguments.find("-isysroot"), std::string::npos) << arguments;
#endif
}

/**
 * system chooses the compiler the system provides over the global default,
 * and the build says where system was named.
 */
TEST_F(CliE2ETest, BuildUsesTheSystemCompilerWhenSystemIsChosen)
{
    installWorkingToolchain("gcc", "15.2.0");
    ASSERT_EQ(runScrap({ "toolchain", "select", "gcc" }, {}, _root).exitCode, 0);
    writeFile("hello/scrap.toml", std::string{ ValidManifest } + "[toolchain]\nname = \"system\"\n");
    writeFile("hello/src/main.cpp", MainSource);

    auto project = runScrap({ "build" }, { realCompiler() }, _root / "hello", BuildTimeout);

    EXPECT_EQ(project.exitCode, 0) << project.stderrText;
    EXPECT_NE(project.stderrText.find("' (from CXX, as system from scrap.toml)\n"), std::string::npos) << project.stderrText;
    EXPECT_EQ(compilersUsed(), "");

    ASSERT_EQ(runScrap({ "toolchain", "select", "system" }, {}, _root).exitCode, 0);
    writeFile("hello/scrap.toml", ValidManifest);
    auto global = runScrap({ "build" }, { realCompiler() }, _root / "hello", BuildTimeout);

    EXPECT_EQ(global.exitCode, 0) << global.stderrText;
    EXPECT_NE(global.stderrText.find("' (from CXX, as system from the global default)\n"), std::string::npos) << global.stderrText;

    auto variable = runScrap({ "build" }, { realCompiler(), "SCRAP_TOOLCHAIN=system" }, _root / "hello", BuildTimeout);

    EXPECT_EQ(variable.exitCode, 0) << variable.stderrText;
    EXPECT_NE(variable.stderrText.find("' (from CXX, as system from SCRAP_TOOLCHAIN)\n"), std::string::npos) << variable.stderrText;
    EXPECT_EQ(compilersUsed(), "");
}

/**
 * A toolchain that is not installed stops the build before anything is
 * compiled, with the command that installs it; nothing is installed in
 * passing.
 */
TEST_F(CliE2ETest, BuildDoesNotStartWithoutTheToolchainItNames)
{
    writeFile("hello/scrap.toml", std::string{ ValidManifest } + "[toolchain]\nname = \"gcc@15.2.0\"\n");
    writeFile("hello/src/main.cpp", MainSource);

    auto result = runScrap({ "build" }, { realCompiler() }, _root / "hello", BuildTimeout);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText, "error: toolchain 'gcc@15.2.0' is not installed\nhint: run 'scrap toolchain install gcc@15.2.0'\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "hello" / "build"));
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * With nothing chosen and no compiler on the system, an installed toolchain
 * is not used in its place, and the build says how to select one.
 */
TEST_F(CliE2ETest, BuildDoesNotChooseAnInstalledToolchainUnasked)
{
    installWorkingToolchain("gcc", "15.2.0");
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);
    std::filesystem::create_directories(_root / "empty");

    auto result = runScrap({ "build" }, { "PATH=" + (_root / "empty").string() }, _root / "hello", BuildTimeout);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: no C++ compiler found\n"
              "hint: select an installed toolchain with 'scrap toolchain select <toolchain>', such as 'gcc@15.2.0'\n");
    EXPECT_EQ(compilersUsed(), "");
}

/**
 * SCRAP_TOOLCHAIN that is neither system nor a specifier is reported.
 */
TEST_F(CliE2ETest, BuildReportsAToolchainVariableItCannotRead)
{
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);

    auto result = runScrap({ "build" }, { "SCRAP_TOOLCHAIN=gcc@15.x" }, _root / "hello", BuildTimeout);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: SCRAP_TOOLCHAIN holds 'gcc@15.x', which is not a toolchain specifier: its version is not numbers joined by dots\n"
              "hint: set it to <family>[@<version>], such as 'gcc@15', or to system, or unset it\n");
}

/**
 * A config.toml that cannot be read stops a build that would read the
 * global default, and no other.
 */
TEST_F(CliE2ETest, BuildReadsTheGlobalDefaultOnlyWhenItNeedsIt)
{
    writeFile("config.toml", "[defaults]\ntoolchain = 15\n");
    writeFile("hello/scrap.toml", ValidManifest);
    writeFile("hello/src/main.cpp", MainSource);
    const std::string config = (_root / "config.toml").string();

    auto needed = runScrap({ "build" }, { realCompiler() }, _root / "hello", BuildTimeout);
    EXPECT_EQ(needed.exitCode, 1);
    EXPECT_EQ(needed.stderrText,
              config + ":2:13: error: defaults.toolchain: must be a string\n"
                       "hint: fix the file, or remove it to clear the settings it holds\n");

    auto passedOver = runScrap({ "build" }, { realCompiler(), "SCRAP_TOOLCHAIN=system" }, _root / "hello", BuildTimeout);
    EXPECT_EQ(passedOver.exitCode, 0) << passedOver.stderrText;
}

namespace {

/**
 * What toolchain install reads and fetches: an index, and the ingots it
 * offers, served from a server on this machine.
 */
struct ToolchainOffer {
    std::string version = {};
    std::string status = "active";
    std::string ingot = {};               ///< What the server sends.
    std::string declaredSha256 = {};      ///< The index's value; that of the ingot when empty.
    std::string extraToolchainKeys = {};  ///< Lines for the toolchain, such as min_scrap_version.
    std::string extraIngotKeys = {};      ///< Lines for the ingot, such as glibc_version.
    bool hasIngot = true;
    bool inRegistry = false;  ///< Keep the ingot in a registry that asks for a token, rather than at a URL.
};

constexpr std::string_view IndexPath = "/index.toml";

std::string hostPlatform()
{
#if defined(__APPLE__)
    return "darwin";
#else
    return "linux";
#endif
}

std::string hostArch()
{
#if defined(__aarch64__)
    return "aarch64";
#else
    return "x86_64";
#endif
}

std::string hostDirectoryName()
{
    return hostPlatform() + "-" + hostArch();
}

std::string sha256Of(const std::string_view bytes)
{
    scrap::Digest::Sha256 digest;
    digest.update(bytes);
    return digest.hexDigest();
}

/** An ingot that holds a compiler whose content tells it apart. */
std::string compilerIngot(const std::string_view content = "compiler")
{
    return scrap::TestSupport::storedXz(scrap::TestTar::fileEntry("bin/g++", content, 0755) + scrap::TestTar::endMarker());
}

/**
 * A server for one family of toolchains: the index, and the ingots at the URLs
 * it gives.
 */
class ToolchainServer {
public:
    ToolchainServer(const std::string_view family, const std::vector<ToolchainOffer>& offers)
        : _server(std::map<std::string, scrap::TestSupport::HttpTestResponse>{})
    {
        offer(family, offers);
    }

    /** Offer @p offers in place of what the index listed. */
    void offer(const std::string_view family, const std::vector<ToolchainOffer>& offers)
    {
        std::string index = "[index]\nschema_version = 1\n";
        for (const ToolchainOffer& offered : offers) {
            index += "\n[[toolchains]]\nfamily = '" + std::string{ family } + "'\nversion = '" + offered.version + "'\nstatus = '" + offered.status
                     + "'\ncompiler = 'g++'\nlicense = 'MIT'\n" + offered.extraToolchainKeys;
            if (offered.hasIngot) {
                index += "\n[[toolchains.ingots]]\nplatform = '" + hostPlatform() + "'\narch = '" + hostArch() + "'\n";
                index += offered.inRegistry ? keepInRegistry(family, offered) : serveAtUrl(offered);
                index += offered.extraIngotKeys;
            }
        }
        _server.respond(std::string{ IndexPath }, { .status = 200, .headers = {}, .body = std::move(index), .stalls = false });
    }

    /** Make the index unreachable: it answers with a failure. */
    void failIndex()
    {
        _server.respond(std::string{ IndexPath }, { .status = 500, .headers = {}, .body = {}, .stalls = false });
    }

    [[nodiscard]] std::string indexVariable() const
    {
        return "SCRAP_TOOLCHAIN_INDEX=" + _server.url(std::string{ IndexPath });
    }

    /** How many requests have reached the server. */
    [[nodiscard]] std::size_t requests() const
    {
        return _server.requestLines().size();
    }

    /** How many times the ingot of @p version was fetched. */
    [[nodiscard]] std::size_t timesFetched(const std::string_view version) const
    {
        const std::string wanted = " " + ingotPath(version) + " ";
        const std::vector<std::string> lines = _server.requestLines();
        return static_cast<std::size_t>(std::ranges::count_if(lines, [&wanted](const std::string& line) {
            return line.find(wanted) != std::string::npos;
        }));
    }

    /** The digest of the manifest of the ingot of @p version kept in the registry. */
    [[nodiscard]] std::string manifestDigest(const std::string_view version) const
    {
        return _manifestDigests.at(std::string{ version });
    }

    /** Whether the stored ingots were asked for, and never with the token. */
    [[nodiscard]] bool storageServedWithoutTheToken() const
    {
        std::vector<std::string> heads = _server.requestHeads();
        std::erase_if(heads, [](const std::string& head) {
            return ! head.starts_with("GET /storage/");
        });
        return (! heads.empty()) && std::ranges::none_of(heads, [](const std::string& head) {
            return head.find("Authorization") != std::string::npos;
        });
    }

private:
    static constexpr std::string_view RegistryToken = "e2e-token";

    [[nodiscard]] static std::string ingotPath(const std::string_view version)
    {
        return "/ingot-" + std::string{ version };
    }

    /** Serve the ingot at a URL, and give the keys of the index that name it. */
    std::string serveAtUrl(const ToolchainOffer& offered)
    {
        const std::string path = ingotPath(offered.version);
        const std::string sha256 = offered.declaredSha256.empty() ? sha256Of(offered.ingot) : offered.declaredSha256;
        _server.respond(path, { .status = 200, .headers = {}, .body = offered.ingot, .stalls = false });
        return "url = '" + _server.url(path) + "'\nsha256 = '" + sha256 + "'\n";
    }

    /**
     * Keep the ingot in a registry as ghcr.io does: behind an anonymous token,
     * with the layer redirected to storage on another host name for this
     * machine. Give the keys of the index that name it.
     */
    std::string keepInRegistry(const std::string_view family, const ToolchainOffer& offered)
    {
        const std::string repository = "skipbit/toolchains/" + std::string{ family };
        const scrap::TestSupport::FakeOciRegistry registry(_server, repository, offered.ingot, sha256Of, { .token = std::string{ RegistryToken }, .service = "e2e", .storagePath = "/storage/" + offered.version, .blob = {} });
        const std::string& digest = registry.manifestDigest();
        _manifestDigests[offered.version] = digest;
        return "registry = '127.0.0.1:" + std::to_string(_server.port()) + "/skipbit/toolchains'\ntag = '" + std::string{ family } + ":" + offered.version + "'\ndigest = '" + digest + "'\n";
    }

    std::map<std::string, std::string> _manifestDigests;

    scrap::TestSupport::HttpTestServer _server;
};

std::string lastLineOf(const std::string& text)
{
    const std::size_t end = text.find_last_not_of('\n');
    return text.substr(text.rfind('\n', end) + 1, end - text.rfind('\n', end));
}

}  // anonymous namespace

/**
 * Text that is no specifier is refused before anything is fetched or kept.
 */
TEST_F(CliE2ETest, InstallRefusesTextThatIsNotASpecifier)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });

    for (const char* text : { "", "@15.2.0", "gcc@", "gcc@15@2", "gcc@15.x" }) {
        auto result = runScrap({ "toolchain", "install", text }, { server.indexVariable() }, _root);

        ASSERT_TRUE(result.exitedNormally) << text;
        EXPECT_EQ(result.exitCode, 1) << text;
        EXPECT_NE(result.stderrText.find("is not a toolchain specifier"), std::string::npos) << result.stderrText;
        EXPECT_NE(result.stderrText.find("hint: write it as <family>[@<version>], such as 'gcc@15'\n"), std::string::npos) << result.stderrText;
    }
    EXPECT_EQ(server.requests(), 0U);
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * The word system names no toolchain to install.
 */
TEST_F(CliE2ETest, InstallRefusesTheWordSystem)
{
    const ToolchainServer server("gcc", {});

    auto result = runScrap({ "toolchain", "install", "system" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText,
              "error: 'system' is not a toolchain specifier: it names the compiler found on the system\n"
              "hint: write it as <family>[@<version>], such as 'gcc@15'\n");
    EXPECT_EQ(server.requests(), 0U);
}

/**
 * The index is fetched and saved, the ingot checked against it, and the
 * toolchain kept below SCRAP_HOME.
 */
TEST_F(CliE2ETest, InstallsAToolchainFromTheIndex)
{
    makeDummy("c++", "exit 0");
    const std::string compilerBefore = readFile("bin/c++");
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot("the installed compiler") } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    const std::string directory = "toolchains/gcc/15.2.0/" + hostDirectoryName();
    EXPECT_EQ(readFile(directory + "/bin/g++"), "the installed compiler");
    EXPECT_NE(result.stderrText.find("Installed 'gcc@15.2.0' at '" + (_root / directory).string() + "'\n"), std::string::npos) << result.stderrText;
    EXPECT_FALSE(readFile("index/toolchain.toml").empty());
    EXPECT_EQ(readFile("bin/c++"), compilerBefore);
    EXPECT_EQ(server.timesFetched("15.2.0"), 1U);
}

/**
 * A toolchain kept in a registry is installed through the registry's token,
 * which the storage its layer moved to never sees, and is recorded by the
 * digest the index gives.
 */
TEST_F(CliE2ETest, InstallsAToolchainKeptInARegistry)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot("the installed compiler"), .inRegistry = true } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    const std::string directory = "toolchains/gcc/15.2.0/" + hostDirectoryName();
    EXPECT_EQ(readFile(directory + "/bin/g++"), "the installed compiler");
    EXPECT_NE(readFile(directory + "/.scrap-toolchain.toml").find(server.manifestDigest("15.2.0")), std::string::npos) << readFile(directory + "/.scrap-toolchain.toml");
    EXPECT_TRUE(server.storageServedWithoutTheToken());
    EXPECT_TRUE(std::filesystem::is_empty(_root / "toolchains" / ".work"));
}

/**
 * A toolchain kept in a registry is told apart from a build of it made again,
 * and replaced by it when forced.
 */
TEST_F(CliE2ETest, InstallTellsOfABuildMadeAgainInARegistryAndReplacesItWhenForced)
{
    ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot("first build"), .inRegistry = true } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root).exitCode, 0);
    server.offer("gcc", { { .version = "15.2.0", .ingot = compilerIngot("second build"), .inRegistry = true } });

    auto told = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);
    auto forced = runScrap({ "toolchain", "install", "--force", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(told.exitCode, 0) << told.stderrText;
    EXPECT_NE(told.stderrText.find("warning: the index now lists a different build of 'gcc@15.2.0'\n"), std::string::npos) << told.stderrText;
    EXPECT_EQ(forced.exitCode, 0) << forced.stderrText;
    EXPECT_EQ(readFile("toolchains/gcc/15.2.0/" + hostDirectoryName() + "/bin/g++"), "second build");
}

/**
 * A shortened specifier takes the newest version it names that is offered.
 */
TEST_F(CliE2ETest, InstallsTheNewestVersionAShortenedSpecifierNames)
{
    const std::string ingot = compilerIngot();
    const std::vector<ToolchainOffer> offers{
        { .version = "15.1.0", .ingot = ingot },
        { .version = "15.2.0", .ingot = ingot },
        { .version = "15.3.0", .status = "disabled", .ingot = ingot },
        { .version = "150.1.0", .ingot = ingot },
    };
    const ToolchainServer server("gcc", offers);

    const std::vector<std::pair<std::string, std::string>> cases{ { "gcc@15", "15.2.0" }, { "gcc@15.1", "15.1.0" }, { "gcc", "150.1.0" } };
    for (const auto& [specifier, version] : cases) {
        auto result = runScrap({ "toolchain", "install", specifier }, { server.indexVariable() }, _root);

        EXPECT_EQ(result.exitCode, 0) << result.stderrText;
        EXPECT_NE(result.stderrText.find("Installed 'gcc@" + version + "' at"), std::string::npos) << result.stderrText;
        EXPECT_TRUE(std::filesystem::exists(_root / "toolchains/gcc" / version / hostDirectoryName() / "bin/g++")) << specifier;
    }
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains/gcc/15.3.0"));
}

/**
 * When nothing matches, the report lists what the index has.
 */
TEST_F(CliE2ETest, InstallListsTheVersionsInTheIndexWhenNoneMatches)
{
    const ToolchainServer server("gcc", { { .version = "15.1.0", .ingot = compilerIngot() }, { .version = "15.2.0", .ingot = compilerIngot() } });

    auto result = runScrap({ "toolchain", "install", "gcc@99" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText, "error: no toolchain matches 'gcc@99'\nhint: available versions include 15.2.0, 15.1.0\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * A download unlike what the index promised is not installed.
 */
TEST_F(CliE2ETest, InstallRefusesADownloadThatDoesNotMatchTheIndex)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot(), .declaredSha256 = sha256Of("something else") } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: the download of 'gcc@15.2.0' does not match the index\n"
                                     "hint: run the command again; if it keeps failing, report it\n"),
              std::string::npos)
        << result.stderrText;
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains/gcc"));
    EXPECT_TRUE(std::filesystem::is_empty(_root / "toolchains/.work"));
    EXPECT_FALSE(std::filesystem::exists(_root / "config.toml"));
}

/**
 * A failure part way through extracting does not look installed afterwards,
 * though members were written before it.
 */
TEST_F(CliE2ETest, InstallLeavesNothingThatLooksInstalledWhenExtractingFails)
{
    const std::string whole = compilerIngot().substr(0, 100);
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = whole } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: cannot install 'gcc@15.2.0': "), std::string::npos) << result.stderrText;
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains/gcc"));
    EXPECT_TRUE(std::filesystem::is_empty(_root / "toolchains/.work"));
    EXPECT_FALSE(std::filesystem::exists(_root / "config.toml"));

    auto selecting = runScrap({ "toolchain", "select", "gcc@15.2.0" }, {}, _root);
    EXPECT_EQ(selecting.exitCode, 1);
    EXPECT_NE(selecting.stderrText.find("is not installed"), std::string::npos) << selecting.stderrText;
}

/**
 * What is installed stays until --force replaces it.
 */
TEST_F(CliE2ETest, InstallLeavesAnInstalledToolchainAloneUnlessForced)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root).exitCode, 0);
    const std::string compiler = "toolchains/gcc/15.2.0/" + hostDirectoryName() + "/bin/g++";
    _temp->writeFile(compiler, "changed since");

    auto again = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(again.stderrText, "'gcc@15.2.0' is already installed at '" + (_root / "toolchains/gcc/15.2.0" / hostDirectoryName()).string() + "'\n");
    EXPECT_EQ(server.timesFetched("15.2.0"), 1U);
    EXPECT_EQ(readFile(compiler), "changed since");

    auto forced = runScrap({ "toolchain", "install", "--force", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(forced.exitCode, 0) << forced.stderrText;
    EXPECT_EQ(server.timesFetched("15.2.0"), 2U);
    EXPECT_EQ(readFile(compiler), "compiler");
}

/**
 * A build of the same version made again is told of, not fetched.
 */
TEST_F(CliE2ETest, InstallTellsOfABuildMadeAgainWithoutFetchingIt)
{
    ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot("first build") } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root).exitCode, 0);
    server.offer("gcc", { { .version = "15.2.0", .ingot = compilerIngot("second build") } });

    auto again = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(again.stderrText,
              "'gcc@15.2.0' is already installed at '" + (_root / "toolchains/gcc/15.2.0" / hostDirectoryName()).string()
                  + "'\n"
                    "warning: the index now lists a different build of 'gcc@15.2.0'\n"
                    "hint: run 'scrap toolchain install --force gcc@15.2.0' to replace it\n");
    EXPECT_EQ(server.timesFetched("15.2.0"), 1U);
}

/**
 * An install whose record has no value to compare says only that it is
 * installed.
 */
TEST_F(CliE2ETest, InstallComparesNothingWhenTheRecordHoldsNoValue)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    installToolchain("gcc", "15.2.0");

    auto again = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(again.stderrText.find("warning:"), std::string::npos) << again.stderrText;
    EXPECT_NE(again.stderrText.find("is already installed at"), std::string::npos) << again.stderrText;
    EXPECT_EQ(server.timesFetched("15.2.0"), 0U);
}

/**
 * An install whose record cannot be read is reported, and --force mends it.
 */
TEST_F(CliE2ETest, InstallReportsARecordItCannotReadAndForceReplacesIt)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    installToolchain("gcc", "15.2.0");
    const std::string record = "toolchains/gcc/15.2.0/" + hostDirectoryName() + "/.scrap-toolchain.toml";
    _temp->writeFile(record, "not a record");

    auto again = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(again.exitCode, 1);
    EXPECT_NE(again.stderrText.find("error: cannot use the installed toolchain: "), std::string::npos) << again.stderrText;
    EXPECT_NE(again.stderrText.find("hint: run 'scrap toolchain install --force gcc@15.2.0'\n"), std::string::npos) << again.stderrText;

    auto forced = runScrap({ "toolchain", "install", "--force", "gcc@15.2.0" }, { server.indexVariable() }, _root);
    EXPECT_EQ(forced.exitCode, 0) << forced.stderrText;
}

/**
 * What build cannot use is not installed, whatever its record says, and install
 * says so as build does.
 */
TEST_F(CliE2ETest, InstallReportsAToolchainWhoseCompilerIsGoneAndForceReplacesIt)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    installToolchain("gcc", "15.2.0");
    const std::filesystem::path compiler = _temp->path() / "toolchains/gcc/15.2.0" / hostDirectoryName() / "bin/g++";
    ASSERT_TRUE(std::filesystem::remove(compiler));

    auto again = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(again.exitCode, 1);
    EXPECT_NE(again.stderrText.find("error: cannot use the installed toolchain: "), std::string::npos) << again.stderrText;
    EXPECT_NE(again.stderrText.find("hint: run 'scrap toolchain install --force gcc@15.2.0'\n"), std::string::npos) << again.stderrText;
    EXPECT_FALSE(std::filesystem::exists(compiler));

    auto forced = runScrap({ "toolchain", "install", "--force", "gcc@15.2.0" }, { server.indexVariable() }, _root);
    EXPECT_EQ(forced.exitCode, 0) << forced.stderrText;
    EXPECT_TRUE(std::filesystem::exists(compiler));
}

/**
 * What the index says of a version does not undo that it is installed.
 */
TEST_F(CliE2ETest, InstallSaysWhatIsInstalledWhateverTheIndexSaysOfIt)
{
    ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    installToolchain("gcc", "15.2.0");
    const std::string installed = "'gcc@15.2.0' is already installed at '";

    const ToolchainServer disabled("gcc", { { .version = "15.2.0", .status = "disabled", .ingot = compilerIngot() } });
    auto stopped = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { disabled.indexVariable() }, _root);
    EXPECT_EQ(stopped.exitCode, 0) << stopped.stderrText;
    EXPECT_NE(stopped.stderrText.find(installed), std::string::npos) << stopped.stderrText;
    EXPECT_EQ(stopped.stderrText.find("no longer provided"), std::string::npos) << stopped.stderrText;

    const ToolchainServer dropped("gcc", { { .version = "16.1.0", .ingot = compilerIngot() } });
    auto gone = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { dropped.indexVariable() }, _root);
    EXPECT_EQ(gone.exitCode, 0) << gone.stderrText;
    EXPECT_NE(gone.stderrText.find(installed), std::string::npos) << gone.stderrText;

    std::filesystem::remove_all(_root / "index");
    server.failIndex();
    auto offline = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);
    EXPECT_EQ(offline.exitCode, 0) << offline.stderrText;
    EXPECT_NE(offline.stderrText.find(installed), std::string::npos) << offline.stderrText;
}

/**
 * Replacing is what the index has to allow, and a shortened specifier means what
 * the index offers.
 */
TEST_F(CliE2ETest, InstallAsksTheIndexWhenForcedOrWhenTheVersionIsShortened)
{
    installToolchain("gcc", "15.2.0");
    const ToolchainServer disabled("gcc", { { .version = "15.2.0", .status = "disabled", .ingot = compilerIngot() } });

    auto forced = runScrap({ "toolchain", "install", "--force", "gcc@15.2.0" }, { disabled.indexVariable() }, _root);
    EXPECT_EQ(forced.exitCode, 1);
    EXPECT_NE(forced.stderrText.find("is no longer provided"), std::string::npos) << forced.stderrText;
    EXPECT_TRUE(std::filesystem::exists(_root / "toolchains/gcc/15.2.0"));

    auto shortened = runScrap({ "toolchain", "install", "gcc@15" }, { disabled.indexVariable() }, _root);
    EXPECT_EQ(shortened.exitCode, 1);
    EXPECT_NE(shortened.stderrText.find("is no longer provided"), std::string::npos) << shortened.stderrText;
}

/**
 * Being installed still makes it the default, whatever the index says of it.
 */
TEST_F(CliE2ETest, InstallSelectsWhatIsInstalledWhenTheIndexNoLongerOffersIt)
{
    installToolchain("gcc", "15.2.0");
    const ToolchainServer disabled("gcc", { { .version = "15.2.0", .status = "disabled", .ingot = compilerIngot() } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { disabled.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    EXPECT_NE(result.stderrText.find("Selected 'gcc@15.2.0' as the global default"), std::string::npos) << result.stderrText;
}

/**
 * A version no longer offered is not installed, and the report names one that
 * is.
 */
TEST_F(CliE2ETest, InstallRefusesAToolchainNoLongerProvided)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() }, { .version = "15.3.0", .status = "disabled", .ingot = compilerIngot() } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.3.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText, "error: 'gcc@15.3.0' is no longer provided\nhint: install another version, such as 'gcc@15.2.0'\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * A version without an ingot for the host is not installed, and the report
 * names the host.
 */
TEST_F(CliE2ETest, InstallRefusesAToolchainWithoutABuildForTheHost)
{
    const ToolchainServer server("gcc", { { .version = "15.1.0", .ingot = compilerIngot() }, { .version = "15.2.0", .hasIngot = false } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText, "error: 'gcc@15.2.0' has no build for " + hostDirectoryName() + "\nhint: available versions for this host include 15.1.0\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * A toolchain that needs a newer scrap is not installed.
 */
TEST_F(CliE2ETest, InstallRefusesAToolchainThatNeedsANewerScrap)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot(), .extraToolchainKeys = "min_scrap_version = '999.0.0'\n" } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_EQ(result.stderrText, "error: 'gcc@15.2.0' needs scrap 999.0.0 or later\nhint: update scrap and run the command again\n");
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * A toolchain that needs exactly the running scrap is installed, which shows
 * the version of the running scrap was read. The version comes from the
 * banner, since a build without release tags is 0.0.0.
 */
TEST_F(CliE2ETest, InstallTakesAToolchainThisScrapIsNewEnoughFor)
{
    const auto version = runScrap({ "--version" }, {}, _root);
    ASSERT_TRUE(version.exitedNormally);
    ASSERT_EQ(version.exitCode, 0) << version.stderrText;
    const std::optional<std::string> running = releaseInBanner(trim(version.stdoutText));
    ASSERT_TRUE(running.has_value()) << "banner: " << version.stdoutText;
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot(), .extraToolchainKeys = "min_scrap_version = '" + *running + "'\n" } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
}

#if defined(__linux__)
/**
 * An ingot that needs a newer glibc than the host has is not installed.
 */
TEST_F(CliE2ETest, InstallRefusesAToolchainThatNeedsANewerGlibc)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot(), .extraIngotKeys = "glibc_version = '999.0'\n" } });

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    const std::string prefix = "error: 'gcc@15.2.0' needs glibc 999.0 or later; this host has ";
    EXPECT_EQ(result.stderrText.substr(0, prefix.size()), prefix) << result.stderrText;
    EXPECT_NE(result.stderrText.find("hint: install an older version, or use a newer host\n"), std::string::npos) << result.stderrText;
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}
#endif

/**
 * An index that cannot be fetched gives way to the saved copy, with a warning,
 * and without one the install fails.
 */
TEST_F(CliE2ETest, InstallFallsBackOnTheSavedIndex)
{
    ToolchainServer server("gcc", { { .version = "15.1.0", .ingot = compilerIngot() }, { .version = "15.2.0", .ingot = compilerIngot() } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.1.0" }, { server.indexVariable() }, _root).exitCode, 0);
    server.failIndex();

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 0) << result.stderrText;
    const std::regex warning{ "^warning: [^\n]*status 500[^\n]*\nwarning: cannot fetch the toolchain index; using the copy fetched at [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\n" };
    EXPECT_TRUE(std::regex_search(result.stderrText, warning)) << result.stderrText;
    EXPECT_TRUE(std::filesystem::exists(_root / "toolchains/gcc/15.2.0"));
}

/**
 * One index does not answer for another: it would bring its own URLs and digests.
 */
TEST_F(CliE2ETest, InstallDoesNotFallBackOnTheCopyOfAnotherIndex)
{
    const ToolchainServer first("gcc", { { .version = "15.1.0", .ingot = compilerIngot() } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.1.0" }, { first.indexVariable() }, _root).exitCode, 0);
    ToolchainServer other("gcc", {});
    other.failIndex();

    auto result = runScrap({ "toolchain", "install", "gcc@16" }, { other.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: cannot fetch the toolchain index: "), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find("using the copy"), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, InstallFailsWhenNoIndexCanBeHad)
{
    ToolchainServer server("gcc", {});
    server.failIndex();

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("error: cannot fetch the toolchain index: "), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("hint: check the network connection and run the command again\n"), std::string::npos) << result.stderrText;
    EXPECT_FALSE(std::filesystem::exists(_root / "toolchains"));
}

/**
 * The first toolchain installed becomes the default and stays it, while
 * versions of a family are kept side by side.
 */
TEST_F(CliE2ETest, InstallMakesTheFirstTheDefaultAndKeepsVersionsSideBySide)
{
    const ToolchainServer server("gcc", { { .version = "15.1.0", .ingot = compilerIngot() }, { .version = "15.2.0", .ingot = compilerIngot() } });

    auto first = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(first.exitCode, 0) << first.stderrText;
    EXPECT_EQ(lastLineOf(first.stderrText), "Selected 'gcc@15.2.0' as the global default, since no toolchain was selected");
    EXPECT_NE(readFile("config.toml").find("toolchain = 'gcc@15.2.0'"), std::string::npos) << readFile("config.toml");

    auto second = runScrap({ "toolchain", "install", "gcc@15.1.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(second.exitCode, 0) << second.stderrText;
    EXPECT_EQ(second.stderrText.find("Selected"), std::string::npos) << second.stderrText;
    EXPECT_NE(readFile("config.toml").find("toolchain = 'gcc@15.2.0'"), std::string::npos) << readFile("config.toml");
    EXPECT_TRUE(std::filesystem::exists(_root / "toolchains/gcc/15.1.0" / hostDirectoryName() / "bin/g++"));
    EXPECT_TRUE(std::filesystem::exists(_root / "toolchains/gcc/15.2.0" / hostDirectoryName() / "bin/g++"));
}

/**
 * A toolchain already installed becomes the default when none is set, without
 * being fetched.
 */
TEST_F(CliE2ETest, InstallSelectsAToolchainAlreadyInstalledWhenNoDefaultIsSet)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root).exitCode, 0);
    ASSERT_EQ(runScrap({ "toolchain", "unselect" }, {}, _root).exitCode, 0);

    auto again = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(again.exitCode, 0) << again.stderrText;
    EXPECT_EQ(lastLineOf(again.stderrText), "Selected 'gcc@15.2.0' as the global default, since no toolchain was selected");
    EXPECT_EQ(server.timesFetched("15.2.0"), 1U);
}

/**
 * A config.toml that cannot be read stops the install before anything is
 * fetched, and is left as it is.
 */
TEST_F(CliE2ETest, InstallStopsAtAConfigItCannotRead)
{
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot() } });
    _temp->writeFile("config.toml", "not = toml =");

    auto result = runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root);

    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("hint: fix the file, or remove it to clear the settings it holds\n"), std::string::npos) << result.stderrText;
    EXPECT_EQ(server.requests(), 0U);
    EXPECT_EQ(readFile("config.toml"), "not = toml =");
}

/**
 * What install keeps is what a build then uses.
 */
TEST_F(CliE2ETest, BuildsWithAToolchainItInstalled)
{
    const std::string compiler = "#!/bin/sh\nexec '" + std::string{ SCRAP_TEST_CXX } + "' \"$@\"\n";
    const ToolchainServer server("gcc", { { .version = "15.2.0", .ingot = compilerIngot(compiler) } });
    ASSERT_EQ(runScrap({ "toolchain", "install", "gcc@15.2.0" }, { server.indexVariable() }, _root).exitCode, 0);
    ASSERT_EQ(runScrap({ "new", "hello" }, {}, _root).exitCode, 0);

    auto built = runScrap({ "build" }, { realCompiler() }, _root / "hello", BuildTimeout);

    EXPECT_EQ(built.exitCode, 0) << built.stderrText;
    EXPECT_NE(built.stderrText.find("Using the toolchain 'gcc@15.2.0'"), std::string::npos) << built.stderrText;
}
