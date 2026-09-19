#include "process/Subprocess.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
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

/**
 * The error the last failed system call left in errno.
 */
auto lastError() -> std::error_code
{
    return {errno, std::generic_category()};
}

/**
 * A file descriptor, closed when it goes out of scope.
 */
class OwnedDescriptor {
public:
    explicit OwnedDescriptor(int descriptor)
        : descriptor_(descriptor)
    {
    }

    OwnedDescriptor(const OwnedDescriptor&) = delete;
    auto operator=(const OwnedDescriptor&) -> OwnedDescriptor& = delete;
    OwnedDescriptor(OwnedDescriptor&&) = delete;
    auto operator=(OwnedDescriptor&&) -> OwnedDescriptor& = delete;

    ~OwnedDescriptor()
    {
        close();
    }

    [[nodiscard]] auto get() const -> int
    {
        return descriptor_;
    }

    void close()
    {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
            descriptor_ = -1;
        }
    }

private:
    int descriptor_;
};

/**
 * Mark @p descriptor to close when a program is started, so only the copies
 * set up for the child reach it.
 */
auto closeOnExec(int descriptor) -> bool
{
    // NOLINTNEXTLINE(hicpp-signed-bitwise) - POSIX fcntl(F_GETFD) result, not a flag combination
    const int flags = ::fcntl(descriptor, F_GETFD);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0;  // NOLINT(hicpp-signed-bitwise) - POSIX flag API
}

/**
 * Describe to posix_spawn where the child's streams and working directory
 * come from.
 *
 * @return 0, or the error of the first action that could not be recorded.
 */
auto recordActions(posix_spawn_file_actions_t& actions,
                   int writeEnd,
                   OutputCapture capture,
                   const std::filesystem::path& workingDirectory) -> int
{
    if (const int result = ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        result != 0) {
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
 * Read @p descriptor until the other end is closed.
 */
void readAll(int descriptor, std::string& output)
{
    std::array<char, ReadChunkBytes> buffer{};
    while (true) {
        const ssize_t count = ::read(descriptor, buffer.data(), buffer.size());
        if (count > 0) {
            output.append(buffer.data(), static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        return;
    }
}

/**
 * Start the program @p arguments name, with its output sent to @p writeEnd.
 *
 * @return The child's process id, or the error that kept it from starting.
 */
auto startProgram(const std::vector<std::string>& arguments,
                  int writeEnd,
                  OutputCapture capture,
                  const std::filesystem::path& workingDirectory)
    -> std::expected<pid_t, std::error_code>  // NOLINT(misc-include-cleaner) - pid_t is provided by <sys/types.h>
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
        return std::unexpected(std::error_code{result, std::generic_category()});
    }
    pid_t child = -1;
    int result = recordActions(actions, writeEnd, capture, workingDirectory);
    if (result == 0) {
        result = ::posix_spawn(&child, argv.front(), &actions, nullptr, argv.data(), environ);
    }
    ::posix_spawn_file_actions_destroy(&actions);
    if (result != 0) {
        return std::unexpected(std::error_code{result, std::generic_category()});
    }
    return child;
}

/**
 * Wait for @p child to end and record in @p completion how it did.
 */
auto waitFor(pid_t child, Completion& completion) -> std::expected<void, std::error_code>
{
    int status = 0;
    pid_t waited = ::waitpid(child, &status, 0);
    while (waited < 0 && errno == EINTR) {
        waited = ::waitpid(child, &status, 0);
    }
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

auto runProgram(const std::vector<std::string>& arguments,
                const std::filesystem::path& workingDirectory,
                const OutputCapture capture) -> std::expected<Completion, std::error_code>
{
    if (arguments.empty()) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    std::array<int, 2> pipeEnds{-1, -1};
    if (::pipe(pipeEnds.data()) != 0) {
        return std::unexpected(lastError());
    }
    OwnedDescriptor readEnd{pipeEnds[0]};
    OwnedDescriptor writeEnd{pipeEnds[1]};
    if (! closeOnExec(readEnd.get()) || ! closeOnExec(writeEnd.get())) {
        return std::unexpected(lastError());
    }

    const auto child = startProgram(arguments, writeEnd.get(), capture, workingDirectory);
    // The parent's copy is closed so the read below ends once the child and
    // anything it started have closed theirs.
    writeEnd.close();
    if (! child.has_value()) {
        return std::unexpected(child.error());
    }

    Completion completion;
    readAll(readEnd.get(), completion.output);
    readEnd.close();
    if (auto waited = waitFor(*child, completion); ! waited.has_value()) {
        return std::unexpected(waited.error());
    }
    return completion;
}

}  // namespace scrap::Process
