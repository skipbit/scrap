// End-to-end tests that spawn the built `scrap` binary as a real subprocess
// and assert on its externally observable behavior (exit code, stdout,
// stderr). These are black-box tests: no scrap:: headers are used here.

#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <poll.h>
#include <regex>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#ifndef SCRAP_BINARY_PATH
#error "SCRAP_BINARY_PATH must be defined by the build (path to the scrap executable)"
#endif

namespace {

/**
 * Whether text is a version banner: the program name, a release triple, and
 * an optional parenthesised description of the build.
 *
 * The check is structural on purpose. Comparing against the exact string the
 * build produced would put the same value on both sides of the assertion, so
 * no wrong derivation could fail it.
 */
bool isVersionBanner(const std::string& text)
{
    static const std::regex Pattern{ R"(^scrap [0-9]+\.[0-9]+\.[0-9]+( \(.+\))?$)" };
    return std::regex_match(text, Pattern);
}

/// A manifest that loads without error.
constexpr std::string_view ValidManifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

/// A manifest whose empty declaration states that the project builds nothing.
constexpr std::string_view ManifestWithoutTargets = "bin = []\n\n[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

/// The source the default layout expects, which gives a project one target.
constexpr std::string_view MainSource = "int main() { return 0; }\n";

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
     * Create a per-test temp directory. Each test case runs as its own
     * ctest entry and ctest may run them in parallel, so the directory
     * name must be unique per test case (and per process, for reruns).
     */
    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        auto dirTemplate = (std::filesystem::temp_directory_path() / (std::string("scrap_e2e_") + info->name() + "_XXXXXX")).string();
        // mkdtemp(3) atomically creates a uniquely-named directory in place
        // of the trailing "XXXXXX", removing the /tmp symlink-preplacement
        // race inherent in "pick a name, then create_directories(name)".
        const char* created = ::mkdtemp(dirTemplate.data());
        ASSERT_NE(created, nullptr) << "mkdtemp failed: " << std::strerror(errno);
        _root = std::filesystem::path(created);
        std::filesystem::create_directories(_root / "bin");
    }

    /**
     * Clean up the temp directory.
     */
    void TearDown() override
    {
        std::filesystem::remove_all(_root);
    }

    /**
     * Write an executable dummy script named @p name into the fixture's
     * bin/ directory, with @p body as the shell script content.
     */
    void makeDummy(const std::string& name, const std::string& body) const
    {
        auto path = _root / "bin" / name;
        {
            std::ofstream out(path);
            out << "#!/bin/sh\n"
                << body << "\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
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
        const auto path = _root / relative;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            ADD_FAILURE() << "cannot create " << path.parent_path() << ": " << ec.message();
            return;
        }
        std::ofstream out(path, std::ios::binary);
        out << content;
        out.close();
        if (! out) {
            ADD_FAILURE() << "cannot write " << path;
        }
    }

    /**
     * The contents of @p relative below the fixture directory, empty when it
     * cannot be read.
     */
    [[nodiscard]] std::string readFile(const std::filesystem::path& relative) const
    {
        std::ifstream in(_root / relative, std::ios::binary);
        return std::string{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
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

        return result;
    }

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
    EXPECT_NE(result.stdoutText.find("USAGE: scrap build [path]"), std::string::npos) << result.stdoutText;
}

TEST_F(CliE2ETest, HelpForAnUnknownCommandFails)
{
    auto result = runScrap({ "help", "nosuch" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("Unknown command: nosuch"), std::string::npos) << result.stderrText;
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
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("Unknown command: \xe6\x97\xa5x\\x1B]0;pwn\\x07"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find('\x1b'), std::string::npos);
    EXPECT_EQ(result.stderrText.find('\x07'), std::string::npos);
}

// --- TS-06: unknown command -------------------------------------------------------

TEST_F(CliE2ETest, UnknownCommand)
{
    auto result = runScrap({ "nonexistent" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
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
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("\xe6\x97\xa5x\\x1B]0;pwn\\x07"), std::string::npos) << result.stderrText;
    EXPECT_EQ(result.stderrText.find('\x1b'), std::string::npos);
    EXPECT_EQ(result.stderrText.find('\x07'), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(_root / "hello"));
}

// --- TS-07: real CLI11 nested subcommand parsing --------------------------------

TEST_F(CliE2ETest, RealCli11Nested)
{
    auto result = runScrap({ "toolchain", "install" }, {}, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("install: not yet implemented"), std::string::npos) << result.stderrText;
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

TEST_F(CliE2ETest, BuildStopsAtTheFirstSourceThatFails)
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
    EXPECT_EQ(calls.find("-c src/main.cpp"), std::string::npos) << calls;
    EXPECT_EQ(calls.find("-o build/debug/bin/app"), std::string::npos) << calls;
    EXPECT_FALSE(std::filesystem::exists(_root / "build" / "debug" / "bin" / "app"));
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

TEST_F(CliE2ETest, BuildReportsALibraryItCannotBuild)
{
    // The database is written first, so an editor still reads the library's
    // sources while the build itself stops.
    writeFile("scrap.toml", "[package]\nname = \"app\"\nversion = \"0.1.0\"\n\n[[lib]]\nname = \"core\"\nsrc = \"src/core.cpp\"\n");
    writeFile("src/core.cpp", "int answer() { return 42; }\n");

    auto result = runScrap({ "build" }, { dummyCompiler() }, _root);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: building the library 'core' is not supported yet\n"), std::string::npos) << result.stderrText;
    EXPECT_NE(readFile("build/debug/compile_commands.json").find("\"file\": \"src/core.cpp\""), std::string::npos);
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
