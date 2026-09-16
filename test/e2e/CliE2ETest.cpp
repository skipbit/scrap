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
auto isVersionBanner(const std::string& text) -> bool
{
    static const std::regex Pattern{R"(^scrap [0-9]+\.[0-9]+\.[0-9]+( \(.+\))?$)"};
    return std::regex_match(text, Pattern);
}

/// A manifest that loads without error.
constexpr std::string_view ValidManifest = "[package]\nname = \"app\"\nversion = \"0.1.0\"\n";

/**
 * Whether @p directory or one of its parents holds a scrap.toml.
 *
 * The tests that expect no project depend on the temp location not sitting
 * inside one, and skip instead of failing on a machine where it does.
 */
auto insideAProject(const std::filesystem::path& directory) -> bool
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

constexpr std::chrono::milliseconds HarnessTimeout{5000};
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
auto trim(std::string_view text) -> std::string
{
    std::size_t begin = 0;
    while (begin < text.size() && (std::isspace(static_cast<unsigned char>(text[begin])) != 0)) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && (std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)) {
        --end;
    }
    return std::string{text.substr(begin, end - begin)};
}

/**
 * Build the child's environment: a minimal, fixed base (SCRAP_HOME, PATH,
 * LC_ALL) so external-command discovery only ever sees this test's fixture
 * directory, plus any caller-supplied "KEY=VALUE" overrides.
 */
auto buildEnv(const std::filesystem::path& scrapHome,
              const std::vector<std::string>& overrides) -> std::vector<std::string>
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
            if (existingEq != std::string::npos && existing.compare(0, existingEq, key) == 0) {
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
auto drainBoth(int fd1, int fd2, std::chrono::steady_clock::time_point deadline, std::string& out1, std::string& out2)
    -> bool
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
            pfds[static_cast<std::size_t>(count)] = {.fd = fd1, .events = POLLIN, .revents = 0};
            ++count;
        }
        if (open2) {
            idx2 = count;
            pfds[static_cast<std::size_t>(count)] = {.fd = fd2, .events = POLLIN, .revents = 0};
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
        if (idx1 >= 0 && (pfds[static_cast<std::size_t>(idx1)].revents & (POLLIN | POLLHUP)) != 0) {
            auto bytesRead = ::read(fd1, buffer.data(), buffer.size());
            if (bytesRead == 0) {
                open1 = false;
            } else if (bytesRead < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    open1 = false;
                }
            } else {
                out1.append(buffer.data(), static_cast<std::size_t>(bytesRead));
            }
        }
        // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX poll() revents flag combination
        if (idx2 >= 0 && (pfds[static_cast<std::size_t>(idx2)].revents & (POLLIN | POLLHUP)) != 0) {
            auto bytesRead = ::read(fd2, buffer.data(), buffer.size());
            if (bytesRead == 0) {
                open2 = false;
            } else if (bytesRead < 0) {
                if (errno != EINTR && errno != EAGAIN) {
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
        auto dirTemplate =
            (std::filesystem::temp_directory_path() / (std::string("scrap_e2e_") + info->name() + "_XXXXXX")).string();
        // mkdtemp(3) atomically creates a uniquely-named directory in place
        // of the trailing "XXXXXX", removing the /tmp symlink-preplacement
        // race inherent in "pick a name, then create_directories(name)".
        const char* created = ::mkdtemp(dirTemplate.data());
        ASSERT_NE(created, nullptr) << "mkdtemp failed: " << std::strerror(errno);
        root_ = std::filesystem::path(created);
        std::filesystem::create_directories(root_ / "bin");
    }

    /**
     * Clean up the temp directory.
     */
    void TearDown() override
    {
        std::filesystem::remove_all(root_);
    }

    /**
     * Write an executable dummy script named @p name into the fixture's
     * bin/ directory, with @p body as the shell script content.
     */
    void makeDummy(const std::string& name, const std::string& body) const
    {
        auto path = root_ / "bin" / name;
        {
            std::ofstream out(path);
            out << "#!/bin/sh\n" << body << "\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    }

    /**
     * Write @p content to @p relative below the fixture directory, creating
     * parent directories.
     */
    void writeFile(const std::filesystem::path& relative, std::string_view content) const
    {
        const auto path = root_ / relative;
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
     * Run the built scrap binary with @p args, a minimal controlled
     * environment (SCRAP_HOME=root_, plus any @p envOverrides), and the
     * given @p cwd. Captures stdout/stderr separately.
     */
    [[nodiscard]] auto runScrap(const std::vector<std::string>& args,
                                const std::vector<std::string>& envOverrides,
                                const std::filesystem::path& cwd) const -> ProcessOutput
    {
        std::array<int, 2> outPipe{-1, -1};
        std::array<int, 2> errPipe{-1, -1};
        if (::pipe(outPipe.data()) != 0) {
            return {};
        }
        if (::pipe(errPipe.data()) != 0) {
            ::close(outPipe[0]);
            ::close(outPipe[1]);
            return {};
        }

        std::vector<std::string> ownedArgs;
        ownedArgs.reserve(args.size() + 1);
        ownedArgs.emplace_back(SCRAP_BINARY_PATH);
        for (const auto& arg : args) {
            ownedArgs.push_back(arg);
        }
        std::vector<char*> argv;
        argv.reserve(ownedArgs.size() + 1);
        for (auto& arg : ownedArgs) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        auto ownedEnv = buildEnv(root_, envOverrides);
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
        auto deadline = std::chrono::steady_clock::now() + HarnessTimeout;
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
        } while (waited < 0 && errno == EINTR);

        if (! timedOut && waited == childPid && WIFEXITED(status)) {
            result.exitedNormally = true;
            result.exitCode = WEXITSTATUS(status);
        }

        return result;
    }

    std::filesystem::path root_;
};

// --- TS-01: builtin resolve + execute ------------------------------------------

TEST_F(CliE2ETest, BuiltinResolveExecute)
{
    auto result = runScrap({"version"}, {}, root_);

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

    auto result = runScrap({"--help"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("External Commands"), std::string::npos);
    EXPECT_NE(result.stdoutText.find("greet"), std::string::npos);
}

// --- TS-03: project scope with no scrap.toml (honest scope: graceful only) ----

TEST_F(CliE2ETest, ProjectScopeNoConfig)
{
    // No scrap.toml is created in root_. StubScriptsReader always returns an
    // empty script list regardless of project contents, so this only proves
    // a config-less project run does not crash and still lists builtins —
    // it does NOT exercise dynamic [scripts] parsing. That is covered by the
    // unit-level ProjectCommandResolverTest against a real ScriptsReader.
    auto result = runScrap({"--help"}, {}, root_);

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

    auto result = runScrap({"version"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(isVersionBanner(trim(result.stdoutText))) << "stdout: " << result.stdoutText;
    EXPECT_NE(result.stderrText.find("warning: command 'version' already registered; ignoring duplicate"),
              std::string::npos);
}

// --- TS-05: global help integration ---------------------------------------------

TEST_F(CliE2ETest, HelpIntegration)
{
    auto result = runScrap({"--help"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    for (const auto* expected : {"USAGE: scrap",
                                 "Built-in Commands",
                                 "Project Commands",
                                 "Toolchain Commands",
                                 "Template Commands",
                                 "See 'scrap help <command>'"}) {
        EXPECT_NE(result.stdoutText.find(expected), std::string::npos) << "missing: " << expected;
    }
}

// --- help command without an argument -------------------------------------------

TEST_F(CliE2ETest, HelpWithoutArgumentListsCommands)
{
    auto result = runScrap({"help"}, {}, root_);
    auto flag = runScrap({"--help"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_NE(result.stdoutText.find("USAGE: scrap"), std::string::npos) << result.stdoutText;
    EXPECT_EQ(result.stdoutText, flag.stdoutText);
}

TEST_F(CliE2ETest, HelpForACommandShowsItsUsage)
{
    auto result = runScrap({"help", "build"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("USAGE: scrap build [path]"), std::string::npos) << result.stdoutText;
}

TEST_F(CliE2ETest, HelpForAnUnknownCommandFails)
{
    auto result = runScrap({"help", "nosuch"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("Unknown command: nosuch"), std::string::npos) << result.stderrText;
}

// --- TS-06: unknown command -------------------------------------------------------

TEST_F(CliE2ETest, UnknownCommand)
{
    auto result = runScrap({"nonexistent"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_NE(result.stderrText.find("Run 'scrap --help' for usage information"), std::string::npos);
}

// --- TS-07: real CLI11 nested subcommand parsing --------------------------------

TEST_F(CliE2ETest, RealCli11Nested)
{
    auto result = runScrap({"toolchain", "install"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.stdoutText.find("install: not yet implemented"), std::string::npos);
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

    auto result = runScrap({"--help"}, {}, root_);

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
         {std::vector<std::string>{"version"}, std::vector<std::string>{"--version"}, std::vector<std::string>{"-V"}}) {
        auto result = runScrap(args, {}, root_);

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
    if (insideAProject(std::filesystem::canonical(root_))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    const std::string searched = std::filesystem::canonical(root_).string();

    auto result = runScrap({"build"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(
        result.stderrText.find("error: could not find scrap.toml in '" + searched + "' or any parent directory\n"),
        std::string::npos)
        << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsAManifestErrorWithItsLocation)
{
    writeFile("scrap.toml", "[package]\nversion = \"0.1.0\"\n");
    const std::string manifest = (std::filesystem::canonical(root_) / "scrap.toml").string();

    auto result = runScrap({"build"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find(manifest + ":1:1: error: package.name: required key is missing\n"),
              std::string::npos)
        << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: "), std::string::npos) << result.stderrText;
}

TEST_F(CliE2ETest, BuildFindsTheProjectAboveTheWorkingDirectory)
{
    writeFile("scrap.toml", ValidManifest);
    std::filesystem::create_directories(root_ / "src" / "detail");

    auto result = runScrap({"build"}, {}, root_ / "src" / "detail");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_NE(result.stdoutText.find("build: not yet implemented"), std::string::npos);
}

TEST_F(CliE2ETest, BuildTakesAPathRelativeToTheWorkingDirectory)
{
    // Run from outside any project, so success can only come from the path.
    if (insideAProject(std::filesystem::canonical(root_))) {
        GTEST_SKIP() << "the temp location is inside a scrap project";
    }
    writeFile("app/scrap.toml", ValidManifest);
    std::filesystem::create_directories(root_ / "elsewhere");

    auto result = runScrap({"build", "../app"}, {}, root_ / "elsewhere");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_NE(result.stdoutText.find("build: not yet implemented"), std::string::npos);
}

TEST_F(CliE2ETest, BuildRejectsAPathThatIsNotADirectory)
{
    // Inside a project, so a search from the missing path would have found one.
    writeFile("scrap.toml", ValidManifest);
    const std::string missing = (std::filesystem::canonical(root_) / "missing").string();

    auto result = runScrap({"build", "missing"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: '" + missing + "' is not a directory\n"), std::string::npos)
        << result.stderrText;
}

TEST_F(CliE2ETest, BuildReportsAPathItCannotAccess)
{
    writeFile("scrap.toml", ValidManifest);
    std::filesystem::create_symlink("loop-b", root_ / "loop-a");
    std::filesystem::create_symlink("loop-a", root_ / "loop-b");
    const std::string loop = (std::filesystem::canonical(root_) / "loop-a").string();

    auto result = runScrap({"build", "loop-a"}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: cannot access '" + loop + "': "), std::string::npos) << result.stderrText;
    EXPECT_NE(result.stderrText.find("\nhint: check the permissions of the path\n"), std::string::npos)
        << result.stderrText;
}

TEST_F(CliE2ETest, BuildRejectsAnEmptyPath)
{
    // Inside a project, so reading the empty path as the working directory would succeed.
    writeFile("scrap.toml", ValidManifest);

    auto result = runScrap({"build", ""}, {}, root_);

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: the path argument is empty\n"), std::string::npos) << result.stderrText;
}

// --- new: creating a project ---------------------------------------------------

TEST_F(CliE2ETest, NewCreatesAProject)
{
    std::filesystem::create_directories(root_ / "work");
    const std::string project = (std::filesystem::canonical(root_ / "work") / "hello").string();

    auto result = runScrap({"new", "hello"}, {}, root_ / "work");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
    EXPECT_EQ(result.stdoutText, "Created project 'hello' at '" + project + "'\n");
    EXPECT_TRUE(std::filesystem::is_regular_file(root_ / "work" / "hello" / "scrap.toml"));
    EXPECT_TRUE(std::filesystem::is_regular_file(root_ / "work" / "hello" / "src" / "main.cpp"));
}

TEST_F(CliE2ETest, NewProjectLoadsInBuild)
{
    std::filesystem::create_directories(root_ / "work");
    auto created = runScrap({"new", "hello"}, {}, root_ / "work");
    ASSERT_EQ(created.exitCode, 0) << created.stderrText;

    auto result = runScrap({"build"}, {}, root_ / "work" / "hello");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_TRUE(result.stderrText.empty()) << result.stderrText;
}

TEST_F(CliE2ETest, NewLeavesAnExistingDirectoryUntouched)
{
    writeFile("work/hello/marker.txt", "keep me\n");
    const std::string project = (std::filesystem::canonical(root_ / "work") / "hello").string();

    auto result = runScrap({"new", "hello"}, {}, root_ / "work");

    ASSERT_TRUE(result.exitedNormally);
    EXPECT_EQ(result.exitCode, 1);
    EXPECT_TRUE(result.stdoutText.empty()) << result.stdoutText;
    EXPECT_NE(result.stderrText.find("error: '" + project + "' already exists\n"), std::string::npos)
        << result.stderrText;
    std::ifstream marker(root_ / "work" / "hello" / "marker.txt", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(marker), std::istreambuf_iterator<char>()), "keep me\n");
    EXPECT_FALSE(std::filesystem::exists(root_ / "work" / "hello" / "scrap.toml"));
}

TEST_F(CliE2ETest, NewRejectsNamesItCannotUse)
{
    std::filesystem::create_directories(root_ / "work");

    for (const char* name : {"", "../x", "a/b"}) {
        auto result = runScrap({"new", name}, {}, root_ / "work");

        ASSERT_TRUE(result.exitedNormally) << name;
        EXPECT_EQ(result.exitCode, 1) << name;
        EXPECT_TRUE(result.stdoutText.empty()) << name << ": " << result.stdoutText;
        EXPECT_NE(result.stderrText.find("error: "), std::string::npos) << name << ": " << result.stderrText;
        EXPECT_NE(result.stderrText.find("\nhint: use up to 64 letters, digits, '-' and '_', starting with a letter\n"),
                  std::string::npos)
            << name << ": " << result.stderrText;
    }
    EXPECT_TRUE(std::filesystem::is_empty(root_ / "work"));
    EXPECT_FALSE(std::filesystem::exists(root_ / "x"));
}
