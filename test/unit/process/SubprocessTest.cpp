#include "process/Subprocess.h"

#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <poll.h>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

using namespace scrap::Process;
using scrap::TestSupport::TempDirectory;

namespace {

/**
 * A command line that runs @p script in the system shell.
 */
std::vector<std::string> shell(const std::string& script)
{
    return { "/bin/sh", "-c", script };
}

/**
 * Options that read back @p capture from a program run in @p directory and
 * wait for it to end.
 */
RunOptions reading(OutputCapture capture, const std::filesystem::path& directory = {})
{
    return { .workingDirectory = directory, .capture = capture, .group = ProcessGroup::Caller, .timeout = std::nullopt, .outputLimit = std::nullopt };
}

/**
 * Options that give a program run in @p directory a group of its own and
 * stop it once a short time has run out.
 */
RunOptions stoppedSoon(const std::filesystem::path& directory)
{
    return { .workingDirectory = directory, .capture = OutputCapture::StandardOutput, .group = ProcessGroup::Own, .timeout = std::chrono::milliseconds{ 100 }, .outputLimit = std::nullopt };
}

/**
 * Options that hand a program the caller's streams and wait for it to end.
 */
RunOptions atTheTerminal()
{
    return { .workingDirectory = {}, .capture = OutputCapture::Terminal, .group = ProcessGroup::Caller, .timeout = std::nullopt, .outputLimit = std::nullopt };
}

/**
 * Puts a pipe in place of one of this process's streams while it lives, and
 * puts the stream back once it goes.
 */
class ReplacedStream {
public:
    explicit ReplacedStream(int stream)
        : _stream(stream)
        , _saved(::dup(stream))
    {
        std::fflush(nullptr);
        EXPECT_EQ(::pipe(_ends.data()), 0);
        ::dup2(stream == STDIN_FILENO ? _ends[0] : _ends[1], stream);
    }

    ReplacedStream(const ReplacedStream&) = delete;
    ReplacedStream& operator=(const ReplacedStream&) = delete;
    ReplacedStream(ReplacedStream&&) = delete;
    ReplacedStream& operator=(ReplacedStream&&) = delete;

    ~ReplacedStream()
    {
        restore();
        ::close(_ends[0]);
        ::close(_ends[1]);
    }

    /**
     * Put the stream back, so what was written to the pipe can be read to
     * its end.
     */
    void restore()
    {
        if (_saved >= 0) {
            ::dup2(_saved, _stream);
            ::close(_saved);
            _saved = -1;
        }
    }

    /**
     * Write @p text for a program that reads the replaced input.
     */
    void write(const std::string& text) const
    {
        EXPECT_EQ(::write(_ends[1], text.data(), text.size()), static_cast<ssize_t>(text.size()));
    }

    /**
     * What was written to the replaced output, once it has been put back.
     */
    std::string written()
    {
        ::close(_ends[1]);
        _ends[1] = -1;
        std::string text;
        std::array<char, 256> buffer{};
        for (ssize_t count = ::read(_ends[0], buffer.data(), buffer.size()); count > 0; count = ::read(_ends[0], buffer.data(), buffer.size())) {
            text.append(buffer.data(), static_cast<std::size_t>(count));
        }
        return text;
    }

private:
    int _stream;
    int _saved;
    std::array<int, 2> _ends{ -1, -1 };
};

/**
 * Create in @p directory a FIFO nothing ever writes to, so a program that
 * reads it waits until it is stopped.
 */
void makeFifoNobodyWrites(const std::filesystem::path& directory)
{
    ASSERT_EQ(::mkfifo((directory / "never").c_str(), 0600), 0);
}

}  // namespace

/**
 * Standard output is read back and standard error is left out.
 */
TEST(SubprocessTest, ReadsStandardOutputAlone)
{
    const auto completion = runProgram(shell("printf out; printf err >&2"), reading(OutputCapture::StandardOutput));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "out");
    EXPECT_EQ(completion->exitCode, 0);
}

/**
 * Both streams are read together, in the order the program wrote them.
 */
TEST(SubprocessTest, ReadsBothStreamsInTheOrderWritten)
{
    const auto completion = runProgram(shell("printf a; printf b >&2; printf c"), reading(OutputCapture::Combined));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "abc");
}

/**
 * A program that exits reports its exit code and no signal.
 */
TEST(SubprocessTest, ReportsTheExitCode)
{
    const auto completion = runProgram(shell("exit 3"), reading(OutputCapture::Combined));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->exitCode, 3);
    EXPECT_FALSE(completion->signal.has_value());
}

/**
 * A program a signal stops reports the signal and no exit code.
 */
TEST(SubprocessTest, ReportsTheSignalThatStoppedIt)
{
    const auto completion = runProgram(shell("kill -KILL $$"), reading(OutputCapture::Combined));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->signal, SIGKILL);
    EXPECT_FALSE(completion->exitCode.has_value());
}

/**
 * The program runs in the directory given.
 */
TEST(SubprocessTest, RunsInTheDirectoryGiven)
{
    const TempDirectory temp;

    const auto completion = runProgram(shell("pwd -P"), reading(OutputCapture::StandardOutput, temp.path()));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, std::filesystem::canonical(temp.path()).string() + "\n");
}

/**
 * Each argument reaches the program as written: no shell splits or expands
 * it on the way.
 */
TEST(SubprocessTest, PassesEachArgumentAsWritten)
{
    const std::vector<std::string> arguments{ "/bin/sh", "-c", R"(printf '%s|' "$@")", "sh", "a b", "", "$HOME", "-c" };

    const auto completion = runProgram(arguments, reading(OutputCapture::StandardOutput));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "a b||$HOME|-c|");
}

/**
 * Nothing is read from the caller's standard input, so a program that reads
 * it finds it empty instead of waiting.
 */
TEST(SubprocessTest, GivesTheProgramNoInput)
{
    const auto completion = runProgram(shell("cat; printf done"), reading(OutputCapture::StandardOutput));

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "done");
}

/**
 * A program that is not there is reported with the system's reason.
 */
TEST(SubprocessTest, ReportsAProgramThatCannotBeStarted)
{
    const TempDirectory temp;
    const std::string absent = (temp.path() / "absent").string();

    const auto completion = runProgram({ absent }, reading(OutputCapture::Combined));

    ASSERT_FALSE(completion.has_value());
    EXPECT_EQ(completion.error(), std::errc::no_such_file_or_directory);
}

/**
 * An empty command line names no program.
 */
TEST(SubprocessTest, RefusesAnEmptyCommandLine)
{
    const auto completion = runProgram({}, reading(OutputCapture::Combined));

    ASSERT_FALSE(completion.has_value());
    EXPECT_EQ(completion.error(), std::errc::invalid_argument);
}

/**
 * A program that ends before its time runs out is not stopped.
 */
TEST(SubprocessTest, LeavesAProgramThatEndsInTimeAlone)
{
    RunOptions options = reading(OutputCapture::StandardOutput);
    options.timeout = std::chrono::seconds{ 30 };

    const auto completion = runProgram(shell("printf done"), options);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "done");
    EXPECT_EQ(completion->exitCode, 0);
    EXPECT_FALSE(completion->timedOut);
}

/**
 * A program still running when its time runs out is killed and reported as
 * such. The program waits on a FIFO nothing writes to, so only the timeout
 * can end it.
 */
TEST(SubprocessTest, StopsAProgramWhoseTimeRunsOut)
{
    const TempDirectory temp;
    makeFifoNobodyWrites(temp.path());

    const auto completion = runProgram(shell("read line < never"), stoppedSoon(temp.path()));

    ASSERT_TRUE(completion.has_value());
    EXPECT_TRUE(completion->timedOut);
    EXPECT_EQ(completion->signal, SIGKILL);
    EXPECT_FALSE(completion->exitCode.has_value());
}

/**
 * A program in a group of its own is stopped with the programs it started.
 * The one it starts holds the write end of a pipe; the read end reaches its
 * end once every holder is gone.
 */
TEST(SubprocessTest, StopsWhatAProgramStartedWithIt)
{
    const TempDirectory temp;
    makeFifoNobodyWrites(temp.path());
    int ends[2] = { -1, -1 };
    ASSERT_EQ(::pipe(ends), 0);

    const auto completion = runProgram(shell("(read line < never) & read line < never"), stoppedSoon(temp.path()));
    ::close(ends[1]);

    ASSERT_TRUE(completion.has_value());
    EXPECT_TRUE(completion->timedOut);
    // The bound only keeps a failure from hanging the test.
    pollfd ready{ .fd = ends[0], .events = POLLIN, .revents = 0 };
    ASSERT_EQ(::poll(&ready, 1, 10000), 1);
    char byte = 0;
    EXPECT_EQ(::read(ends[0], &byte, 1), 0);
    ::close(ends[0]);
}

/**
 * A program that exits while what it started still holds its output open is
 * stopped with what it started once the time runs out.
 */
TEST(SubprocessTest, StopsWhatAProgramLeftHoldingItsOutput)
{
    const TempDirectory temp;
    makeFifoNobodyWrites(temp.path());
    int ends[2] = { -1, -1 };
    ASSERT_EQ(::pipe(ends), 0);

    const auto completion = runProgram(shell("(read line < never) & exit 0"), stoppedSoon(temp.path()));
    ::close(ends[1]);

    ASSERT_TRUE(completion.has_value());
    EXPECT_TRUE(completion->timedOut);
    // The bound only keeps a failure from hanging the test.
    pollfd ready{ .fd = ends[0], .events = POLLIN, .revents = 0 };
    ASSERT_EQ(::poll(&ready, 1, 10000), 1);
    char byte = 0;
    EXPECT_EQ(::read(ends[0], &byte, 1), 0);
    ::close(ends[0]);
}

/**
 * Output past the limit is not read, and a program that keeps writing sees
 * its output closed rather than waiting on it.
 */
TEST(SubprocessTest, StopsReadingAtTheLimit)
{
    RunOptions options = reading(OutputCapture::StandardOutput);
    options.outputLimit = 5;

    const auto completion = runProgram(shell("yes"), options);

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->output, "y\ny\ny");
    EXPECT_FALSE(completion->timedOut);
}

/**
 * A program at the terminal reads the caller's standard input.
 */
TEST(SubprocessTest, GivesAProgramAtTheTerminalTheCallersInput)
{
    ReplacedStream input{ STDIN_FILENO };
    input.write("hello\n");

    const auto completion = runProgram(shell(R"(read line; [ "$line" = hello ] && exit 7)"), atTheTerminal());
    input.restore();

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->exitCode, 7);
}

/**
 * A program at the terminal writes to the caller's standard output and
 * standard error, and nothing is read back.
 */
TEST(SubprocessTest, LeavesTheOutputOfAProgramAtTheTerminalToTheCaller)
{
    ReplacedStream output{ STDOUT_FILENO };
    ReplacedStream error{ STDERR_FILENO };

    const auto completion = runProgram(shell("printf out; printf err >&2"), atTheTerminal());
    output.restore();
    error.restore();

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->exitCode, 0);
    EXPECT_EQ(completion->output, "");
    EXPECT_EQ(output.written(), "out");
    EXPECT_EQ(error.written(), "err");
}

/**
 * An interrupt and a quit sent while a program is at the terminal leave the
 * caller running, and the caller's handling of both is put back afterwards.
 */
TEST(SubprocessTest, KeepsTheCallerThroughAnInterruptWhileAProgramIsAtTheTerminal)
{
    struct sigaction before { };
    ASSERT_EQ(::sigaction(SIGINT, nullptr, &before), 0);

    const auto completion = runProgram(shell("kill -INT $PPID; kill -QUIT $PPID; exit 0"), atTheTerminal());

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->exitCode, 0);
    struct sigaction after { };
    ASSERT_EQ(::sigaction(SIGINT, nullptr, &after), 0);
    EXPECT_EQ(after.sa_handler, before.sa_handler);
}

/**
 * A program at the terminal is stopped by an interrupt, even though the
 * caller ignores it meanwhile.
 */
TEST(SubprocessTest, LetsAnInterruptStopAProgramAtTheTerminal)
{
    const auto completion = runProgram(shell("kill -INT $$; exit 0"), atTheTerminal());

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->signal, SIGINT);
}

/**
 * A program at the terminal is stopped by a quit, even though the caller
 * ignores it meanwhile.
 */
TEST(SubprocessTest, LetsAQuitStopAProgramAtTheTerminal)
{
    // bash, which is /bin/sh on some systems, ignores a quit itself, so the
    // shell hands its process over to kill. No core file is left behind.
    const auto completion = runProgram(shell("ulimit -c 0; exec kill -QUIT $$"), atTheTerminal());

    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->signal, SIGQUIT);
}
