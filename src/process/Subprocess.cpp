#include "process/Subprocess.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <poll.h>
// <csignal> does not reliably resolve killpg()/SIGKILL for include-cleaner on
// all platforms; <signal.h> is the POSIX header that actually declares them.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
// NOLINTNEXTLINE(misc-include-cleaner) - only pulled in on Darwin, guarded by __APPLE__
#include <crt_externs.h>  // _NSGetEnviron(): the only correct way to reach environ on Darwin
#define environ (*_NSGetEnviron())
#endif

namespace scrap::Process {

namespace {

constexpr std::size_t ReadChunkBytes = 4096;

// Pause between checks on a program that has closed its output but not yet
// exited, while its time has not run out.
constexpr int ReapPollIntervalMs = 5;

using Clock = std::chrono::steady_clock;

/**
 * The error the last failed system call left in errno.
 */
std::error_code lastError()
{
    return { errno, std::generic_category() };
}

/**
 * A file descriptor, closed when it goes out of scope.
 */
class OwnedDescriptor {
public:
    explicit OwnedDescriptor(int descriptor)
        : _descriptor(descriptor)
    {
    }

    OwnedDescriptor(const OwnedDescriptor&) = delete;
    OwnedDescriptor& operator=(const OwnedDescriptor&) = delete;
    OwnedDescriptor(OwnedDescriptor&&) = delete;
    OwnedDescriptor& operator=(OwnedDescriptor&&) = delete;

    ~OwnedDescriptor()
    {
        close();
    }

    [[nodiscard]] int get() const
    {
        return _descriptor;
    }

    void close()
    {
        if (_descriptor >= 0) {
            ::close(_descriptor);
            _descriptor = -1;
        }
    }

private:
    int _descriptor;
};

#if defined(__APPLE__)
/**
 * Mark @p descriptor to close when a program is started, so only the copies
 * set up for the child reach it. Only Darwin needs this: it has no call that
 * creates a pipe already marked.
 */
bool closeOnExec(int descriptor)
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX fcntl(F_GETFD) result, not a flag combination
    const int flags = ::fcntl(descriptor, F_GETFD);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0;  // NOLINT(hicpp-signed-bitwise) - POSIX flag API
}
#endif

/**
 * Describe to posix_spawn where the child's streams and working directory
 * come from.
 *
 * @return 0, or the error of the first action that could not be recorded.
 */
int recordActions(posix_spawn_file_actions_t& actions,
                  int writeEnd,
                  OutputCapture capture,
                  const std::filesystem::path& workingDirectory)
{
    if (const int result = ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0); result != 0) {
        return result;
    }
    if (const int result = ::posix_spawn_file_actions_adddup2(&actions, writeEnd, STDOUT_FILENO); result != 0) {
        return result;
    }
    const int errorResult = capture == OutputCapture::Combined
                                ? ::posix_spawn_file_actions_adddup2(&actions, writeEnd, STDERR_FILENO)
                                : ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    if (errorResult != 0) {
        return errorResult;
    }
    if (! workingDirectory.empty()) {
        return ::posix_spawn_file_actions_addchdir_np(&actions, workingDirectory.c_str());
    }
    return 0;
}

/**
 * Read @p descriptor until the other end is closed, @p limit bytes have been
 * read, or @p deadline passes.
 *
 * @return An empty code once reading stopped for one of those reasons, or
 *         why it stopped otherwise: what came back until then is part of
 *         what the program wrote, not all of it.
 */
std::error_code readOutput(int descriptor, std::optional<Clock::time_point> deadline, std::optional<std::size_t> limit, std::string& output)
{
    std::array<char, ReadChunkBytes> buffer{};
    while ((! limit.has_value()) || (output.size() < *limit)) {
        int waitMs = -1;
        if (deadline.has_value()) {
            const auto remaining = *deadline - Clock::now();
            if (remaining <= Clock::duration::zero()) {
                return {};
            }
            // NOLINTNEXTLINE(misc-include-cleaner) - std::chrono::ceil is provided by <chrono>
            waitMs = static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
        }

        // NOLINTNEXTLINE(misc-include-cleaner) - struct pollfd is provided by <poll.h>
        struct pollfd ready { };
        ready.fd = descriptor;
        ready.events = POLLIN;  // NOLINT(misc-include-cleaner) - POLLIN is provided by <poll.h>
        // NOLINTNEXTLINE(misc-include-cleaner) - poll() is provided by <poll.h>
        const int polled = ::poll(&ready, 1, waitMs);
        if (polled == 0) {
            return {};
        }
        if (polled < 0) {
            if (errno == EINTR) {
                continue;
            }
            return lastError();
        }

        const ssize_t count = ::read(descriptor, buffer.data(), buffer.size());
        if (count > 0) {
            const std::size_t wanted = limit.has_value() ? std::min(static_cast<std::size_t>(count), *limit - output.size()) : static_cast<std::size_t>(count);
            output.append(buffer.data(), wanted);
            continue;
        }
        if (count == 0) {
            return {};
        }
        if ((errno == EINTR) || (errno == EAGAIN)) {
            continue;
        }
        return lastError();
    }
    return {};
}

/**
 * Create a pipe both of whose ends close when a program is started.
 *
 * Where the system creates it in one step, nothing between the two can
 * inherit an end: a program started from elsewhere at that moment would
 * otherwise hold the write end open, and the read below would wait for it
 * rather than for the child.
 */
std::error_code openPipe(std::array<int, 2>& ends)
{
#if defined(__APPLE__)
    if (::pipe(ends.data()) != 0) {
        return lastError();
    }
    if ((! closeOnExec(ends[0])) || (! closeOnExec(ends[1]))) {
        return lastError();
    }
    return {};
#else
    if (::pipe2(ends.data(), O_CLOEXEC) != 0) {
        return lastError();
    }
    return {};
#endif
}

/**
 * Start the program @p arguments name, with its output sent to @p writeEnd.
 *
 * @return The child's process id, or the error that kept it from starting.
 */
std::expected<pid_t, std::error_code> startProgram(const std::vector<std::string>& arguments,  // NOLINT(misc-include-cleaner) - pid_t is provided by <sys/types.h>
                                                   int writeEnd,
                                                   const RunOptions& options)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments) {
        // posix_spawn takes char* const[] for historical reasons and does not
        // write through it.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    if (const int result = ::posix_spawn_file_actions_init(&actions); result != 0) {
        return std::unexpected(std::error_code{ result, std::generic_category() });
    }
    posix_spawnattr_t attributes;
    if (const int result = ::posix_spawnattr_init(&attributes); result != 0) {
        ::posix_spawn_file_actions_destroy(&actions);
        return std::unexpected(std::error_code{ result, std::generic_category() });
    }
    pid_t child = -1;
    int result = recordActions(actions, writeEnd, options.capture, options.workingDirectory);
    if ((result == 0) && (options.group == ProcessGroup::Own)) {
        // A group id of 0 makes the child the leader of a new group.
        result = ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        if (result == 0) {
            result = ::posix_spawnattr_setpgroup(&attributes, 0);
        }
    }
    if (result == 0) {
        result = ::posix_spawn(&child, argv.front(), &actions, &attributes, argv.data(), environ);
    }
    ::posix_spawnattr_destroy(&attributes);
    ::posix_spawn_file_actions_destroy(&actions);
    if (result != 0) {
        return std::unexpected(std::error_code{ result, std::generic_category() });
    }
    return child;
}

/**
 * Wait for @p child to end, however long it takes.
 *
 * @return What waitpid returned: the child's id once it has been waited for.
 */
pid_t waitUntilEnded(pid_t child, int& status)
{
    pid_t waited = ::waitpid(child, &status, 0);
    while ((waited < 0) && (errno == EINTR)) {
        waited = ::waitpid(child, &status, 0);
    }
    return waited;
}

/**
 * Kill @p child, with its process group when @p group is its own.
 *
 * The child is not yet waited for, so its group id cannot have been reused.
 */
void stopProgram(pid_t child, ProcessGroup group)
{
    // NOLINTBEGIN(misc-include-cleaner) - killpg(), kill() and SIGKILL are provided by <signal.h>
    if (group == ProcessGroup::Own) {
        ::killpg(child, SIGKILL);
    } else {
        ::kill(child, SIGKILL);
    }
    // NOLINTEND(misc-include-cleaner)
}

/**
 * Wait for @p child to end until @p deadline passes, then kill it and wait
 * for that. Closed output alone does not mean the child has exited, so one
 * that exits soon after closing it is still waited for until the deadline.
 *
 * @return What waitpid returned: the child's id once it has been waited for.
 */
pid_t waitUntilDeadline(pid_t child, Clock::time_point deadline, ProcessGroup group, int& status, bool& timedOut)
{
    while (true) {
        // NOLINTNEXTLINE(misc-include-cleaner) - WNOHANG is provided by <sys/wait.h>
        const pid_t waited = ::waitpid(child, &status, WNOHANG);
        if ((waited < 0) && (errno == EINTR)) {
            continue;
        }
        if (waited != 0) {
            return waited;
        }
        if (Clock::now() >= deadline) {
            stopProgram(child, group);
            timedOut = true;
            return waitUntilEnded(child, status);
        }
        ::poll(nullptr, 0, ReapPollIntervalMs);
    }
}

/**
 * Wait for @p child to end and record in @p completion how it did.
 *
 * With a @p deadline, a child still running when it passes is killed.
 */
std::expected<void, std::error_code> waitFor(pid_t child, std::optional<Clock::time_point> deadline, ProcessGroup group, Completion& completion)
{
    int status = 0;
    const pid_t waited = deadline.has_value() ? waitUntilDeadline(child, *deadline, group, status, completion.timedOut)
                                              : waitUntilEnded(child, status);
    if (waited != child) {
        return std::unexpected(lastError());
    }

    // NOLINTBEGIN(misc-include-cleaner) - the wait status macros are provided by <sys/wait.h>
    if (WIFEXITED(status)) {
        completion.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        completion.signal = WTERMSIG(status);
    }
    // NOLINTEND(misc-include-cleaner)
    return {};
}

}  // anonymous namespace

std::expected<Completion, std::error_code> runProgram(const std::vector<std::string>& arguments, const RunOptions& options)
{
    if (arguments.empty()) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    std::array<int, 2> pipeEnds{ -1, -1 };
    if (const std::error_code failed = openPipe(pipeEnds)) {
        return std::unexpected(failed);
    }
    OwnedDescriptor readEnd{ pipeEnds[0] };
    OwnedDescriptor writeEnd{ pipeEnds[1] };

    const auto child = startProgram(arguments, writeEnd.get(), options);
    // The parent's copy is closed so the read below ends once the child and
    // anything it started have closed theirs.
    writeEnd.close();
    if (! child.has_value()) {
        return std::unexpected(child.error());
    }

    std::optional<Clock::time_point> deadline;
    if (options.timeout.has_value()) {
        deadline = Clock::now() + *options.timeout;
    }

    Completion completion;
    const std::error_code read = readOutput(readEnd.get(), deadline, options.outputLimit, completion.output);
    readEnd.close();

    // The child is waited for whichever way the read ended, so no program is
    // left behind, and a read that stopped early is reported rather than
    // passed off as everything the program wrote.
    const auto waited = waitFor(*child, deadline, options.group, completion);
    if (read) {
        return std::unexpected(read);
    }
    if (! waited.has_value()) {
        return std::unexpected(waited.error());
    }
    return completion;
}

}  // namespace scrap::Process
