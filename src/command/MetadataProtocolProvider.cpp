#include "command/MetadataProtocolProvider.h"

#include "command/ExternalMetadataProvider.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
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
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>

#if defined(__APPLE__)
// NOLINTNEXTLINE(misc-include-cleaner) - only pulled in on Darwin, guarded by __APPLE__
#include <crt_externs.h>  // _NSGetEnviron(): the only correct way to reach environ on Darwin
#define environ (*_NSGetEnviron())
#endif

namespace scrap::Command {

namespace {

// --- Protocol constants -------------------------------------------------------

constexpr const char* ProtocolFlag = "--scrap-metadata";
constexpr const char* HelpFlag = "--help";

// --- Subprocess capture tuning -------------------------------------------------

// Upper bound on captured stdout; a metadata description is a single short
// line, so this is generous headroom rather than an expected size.
constexpr std::size_t MaxCaptureBytes = 64UL * 1024UL;
constexpr std::size_t ReadChunkBytes = 4096;

// Short sleep between non-blocking reap polls while waiting for a child that
// has already closed stdout to actually exit (see the deadline-bounded reap
// in runOnce()). Small enough not to add perceptible latency, large enough
// to avoid busy-waiting.
constexpr int ReapPollIntervalMs = 5;

/**
 * Result of running an executable once and capturing its stdout.
 */
struct SubprocessResult {
    bool exitedNormally = false;
    int exitCode = -1;
    std::string capturedStdout;
};

/**
 * Extract the first non-empty, whitespace-trimmed line from @p text.
 *
 * Returns an empty string if every line is blank (or @p text is empty).
 */
auto firstNonEmptyLine(std::string_view text) -> std::string
{
    std::size_t pos = 0;
    while (pos <= text.size()) {
        auto newlinePos = text.find('\n', pos);
        auto lineEnd = (newlinePos == std::string_view::npos) ? text.size() : newlinePos;
        auto line = text.substr(pos, lineEnd - pos);

        while (! line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1);
        }
        while (! line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }

        if (! line.empty()) {
            return std::string{line};
        }
        if (newlinePos == std::string_view::npos) {
            break;
        }
        pos = newlinePos + 1;
    }
    return {};
}

/**
 * Mark @p fd close-on-exec so it never leaks into an unrelated child.
 *
 * Explicit dup2 targets set up via posix_spawn_file_actions are unaffected:
 * dup2 always clears FD_CLOEXEC on the newly created descriptor.
 */
auto setCloseOnExec(int fd) -> bool
{
    auto flags =
        ::fcntl(fd, F_GETFD);  // NOLINT(hicpp-signed-bitwise) - POSIX fcntl(F_GETFD) result, not a flag combination
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;  // NOLINT(hicpp-signed-bitwise) - POSIX fcntl flag API
}

/**
 * RAII wrapper around a POSIX file descriptor. Move-only; closes a valid
 * (>= 0) descriptor in its destructor so every exit path out of runOnce()
 * — including one taken because of an exception thrown between pipe()
 * creation and the descriptor's last explicit use — closes it exactly once.
 */
class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd)
        : fd_(fd)
    {
    }

    UniqueFd(const UniqueFd&) = delete;
    auto operator=(const UniqueFd&) -> UniqueFd& = delete;

    UniqueFd(UniqueFd&& other) noexcept
        : fd_(other.release())
    {
    }
    auto operator=(UniqueFd&& other) noexcept -> UniqueFd&
    {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~UniqueFd()
    {
        reset();
    }

    [[nodiscard]] auto get() const -> int
    {
        return fd_;
    }

    /** Relinquish ownership, returning the raw descriptor without closing it. */
    [[nodiscard]] auto release() -> int
    {
        auto fd = fd_;
        fd_ = -1;
        return fd;
    }

    /** Close the current descriptor (if any) and take ownership of @p fd. */
    void reset(int fd = -1)
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

/**
 * Drain @p readFd into @p out until EOF, the capture cap is hit, the
 * deadline passes, or an unexpected poll()/read() error occurs. Uses
 * poll() so a full pipe can never deadlock the caller (the child keeps
 * making progress as we keep reading).
 *
 * The caller (runOnce) reaps the child (bounded by the same deadline) once
 * this returns, regardless of which of the above reasons stopped the drain,
 * so no return value is needed to single out "timed out" from the others.
 */
auto drainUntilEofOrDeadline(int readFd, std::chrono::steady_clock::time_point deadline, std::string& out) -> void
{
    std::array<char, ReadChunkBytes> buffer{};

    while (true) {
        auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return;  // Deadline reached; the caller reaps the child, bounded by the deadline.
        }

        // NOLINTNEXTLINE(misc-include-cleaner) - std::chrono::ceil is provided by <chrono>
        auto remainingMs = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
        // NOLINTNEXTLINE(misc-include-cleaner) - struct pollfd is provided by <poll.h>
        struct pollfd pfd { };
        pfd.fd = readFd;
        pfd.events = POLLIN;  // NOLINT(misc-include-cleaner) - POLLIN is provided by <poll.h>

        // NOLINTNEXTLINE(misc-include-cleaner) - poll() is provided by <poll.h>
        const int pollResult = ::poll(&pfd, 1, static_cast<int>(remainingMs));
        if (pollResult == 0) {
            return;  // Timed out waiting for the next chunk.
        }
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;  // Unexpected poll() failure: stop reading.
        }

        // NOLINTNEXTLINE(hicpp-signed-bitwise, misc-include-cleaner) - POLLHUP is provided by <poll.h>
        if ((pfd.revents & (POLLIN | POLLHUP)) == 0) {
            // NOLINTNEXTLINE(hicpp-signed-bitwise, misc-include-cleaner) - POLLERR is provided by <poll.h>
            if ((pfd.revents & POLLERR) != 0) {
                return;
            }
            continue;
        }

        auto bytesRead = ::read(readFd, buffer.data(), buffer.size());
        if (bytesRead == 0) {
            // EOF: the child closed its stdout. It may have exited already,
            // or it may still be running (e.g. `exec 1>&-; sleep ...`) — the
            // caller's deadline-bounded reap handles both uniformly.
            return;
        }
        if (bytesRead < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            return;  // Unexpected read() failure: stop reading.
        }

        if (out.size() >= MaxCaptureBytes) {
            return;  // Capture cap reached; stop reading and let the caller reap.
        }
        auto available = MaxCaptureBytes - out.size();
        auto toAppend = std::min(static_cast<std::size_t>(bytesRead), available);
        out.append(buffer.data(), toAppend);
    }
}

/**
 * Reap @p childPid, bounded by @p deadline, and return its exit code if it
 * exited normally in time.
 *
 * EOF on the child's stdout (where draining stops) proves only that the write
 * end is closed, not that the child has exited — a valid probe can legitimately
 * `echo ...; exec 1>&-; sleep 0.05; exit 0`, closing stdout while still doing
 * brief work. Killing unconditionally at that point would race a
 * still-alive-but-about-to-exit child and discard its real (already-captured)
 * output for a bogus WIFSIGNALED status. So the reap instead: (1) tries a
 * non-blocking waitpid() first — a child that has already exited (the common
 * case) is reaped at once with its real status; (2) if the child is still
 * running, keeps polling (bounded by @p deadline) so one that closed stdout
 * early but exits shortly after is still reaped with its real status; (3) a
 * child still alive at the deadline is SIGKILLed (its whole process group) and
 * then waited for, bounding the reap so it can never block for the child's full
 * lifetime. killpg (not kill) SIGKILLs the whole process group, including
 * grandchildren the probe spawned, but waitpid(childPid) only reaps the direct
 * child itself — any signalled grandchildren are reparented to init, which
 * reaps them. Killing before the post-kill waitpid (not after) avoids a
 * pid/pgid-reuse race, since the still-unreaped group leader keeps pgid valid
 * and unique.
 *
 * @return the exit code if the child exited normally within the deadline, or
 *   std::nullopt if it had to be killed (or could not be reaped).
 */
auto reapBounded(pid_t childPid, std::chrono::steady_clock::time_point deadline) -> std::optional<int>
{
    int status = 0;
    pid_t waited = 0;
    bool killed = false;
    while (true) {
        // NOLINTNEXTLINE(misc-include-cleaner) - WNOHANG is provided by <sys/wait.h>
        waited = ::waitpid(childPid, &status, WNOHANG);
        if (waited == childPid) {
            break;  // Reaped; status is valid.
        }
        if (waited < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;  // e.g. ECHILD; nothing more we can do.
        }
        // waited == 0: still running.
        if (std::chrono::steady_clock::now() >= deadline) {
            // NOLINTNEXTLINE(misc-include-cleaner) - killpg() and SIGKILL are provided by <signal.h>
            ::killpg(childPid, SIGKILL);
            killed = true;
            waited = ::waitpid(childPid, &status, 0);
            while (waited < 0 && errno == EINTR) {
                waited = ::waitpid(childPid, &status, 0);
            }
            break;
        }
        ::poll(nullptr, 0, ReapPollIntervalMs);  // Portable short sleep; avoid busy-waiting.
    }

    // NOLINTNEXTLINE(misc-include-cleaner) - WIFEXITED/WEXITSTATUS are provided by <sys/wait.h>
    if (! killed && waited == childPid && WIFEXITED(status)) {
        return WEXITSTATUS(status);  // NOLINT(misc-include-cleaner) - provided by <sys/wait.h>
    }
    return std::nullopt;
}

/**
 * Run @p executable with a single @p flag argument (no shell, argv-array
 * only), capturing stdout while discarding stdin/stderr. The child runs in
 * its own process group and is reaped via reapBounded(), so a slow or hung
 * probe can never block the caller past @p timeout.
 *
 * Every path below closes any fds it opened (via UniqueFd's RAII) and, once
 * posix_spawn has created a child, reaps it — no fd leaks, no zombies.
 */
auto runOnce(const std::filesystem::path& executable,
             const char* flag,
             std::chrono::milliseconds timeout) -> SubprocessResult
{
    std::array<int, 2> pipeFds{-1, -1};
    if (::pipe(pipeFds.data()) != 0) {
        return {};
    }
    UniqueFd readFd{pipeFds[0]};
    UniqueFd writeFd{pipeFds[1]};

    if (! setCloseOnExec(readFd.get()) || ! setCloseOnExec(writeFd.get())) {
        return {};
    }

    posix_spawn_file_actions_t fileActions;
    posix_spawn_file_actions_init(&fileActions);
    posix_spawn_file_actions_addopen(&fileActions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fileActions, writeFd.get(), STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&fileActions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    // Explicit close of the read end for clarity; FD_CLOEXEC already closes
    // both pipe fds at exec() time, so this is redundant-but-documented.
    posix_spawn_file_actions_addclose(&fileActions, readFd.get());

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);  // New, independent process group (pgid == child pid).

    const std::string exePath = executable.string();
    std::array<const char*, 3> argv{exePath.c_str(), flag, nullptr};
    // posix_spawn's argv parameter is char* const[] for historical POSIX
    // reasons; the spawned process never mutates argv.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto** spawnArgv = const_cast<char**>(argv.data());

    pid_t childPid = -1;
    const int spawnStatus = ::posix_spawn(&childPid, exePath.c_str(), &fileActions, &attr, spawnArgv, environ);

    posix_spawn_file_actions_destroy(&fileActions);
    posix_spawnattr_destroy(&attr);

    writeFd.reset();  // Parent's copy: read() below must see EOF once the child is done.

    if (spawnStatus != 0) {
        return {};
    }

    SubprocessResult result;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    drainUntilEofOrDeadline(readFd.get(), deadline, result.capturedStdout);
    readFd.reset();

    // Reap the child, bounded by the deadline (see reapBounded). Its process
    // group leader is childPid; any write it attempts after we closed readFd
    // yields a harmless SIGPIPE.
    if (auto exitCode = reapBounded(childPid, deadline)) {
        result.exitedNormally = true;
        result.exitCode = *exitCode;
    }

    return result;
}

/**
 * Run @p executable with @p flag and return its first-line description, or
 * an empty string if the attempt did not yield a usable result (non-zero
 * exit, empty output, spawn failure, or timeout).
 */
auto probe(const std::filesystem::path& executable, const char* flag, std::chrono::milliseconds timeout) -> std::string
{
    auto result = runOnce(executable, flag, timeout);
    if (! result.exitedNormally || result.exitCode != 0) {
        return {};
    }
    return firstNonEmptyLine(result.capturedStdout);
}

}  // namespace

/**
 * Construct with the subprocess timeout used for both probe attempts.
 */
MetadataProtocolProvider::MetadataProtocolProvider(std::chrono::milliseconds timeout)
    : timeout_(timeout)
{
}

/**
 * Fetch metadata via --scrap-metadata, falling back to --help.
 *
 * The executable path is canonicalized so posix_spawn always receives a
 * path containing a slash (no PATH search, no shell, no injection surface).
 * Only a plain-text first-line description is extracted; structured
 * (name/options) metadata is not yet part of the protocol.
 */
auto MetadataProtocolProvider::fetch(const std::filesystem::path& executable)
    -> std::expected<ExternalCommandMetadata, std::string>
{
    std::error_code ec;
    auto canonicalized = std::filesystem::weakly_canonical(executable, ec);
    const std::filesystem::path& exe = ec ? executable : canonicalized;

    if (auto description = probe(exe, ProtocolFlag, timeout_); ! description.empty()) {
        return ExternalCommandMetadata{.name = "", .description = std::move(description), .options = {}};
    }

    if (auto description = probe(exe, HelpFlag, timeout_); ! description.empty()) {
        return ExternalCommandMetadata{.name = "", .description = std::move(description), .options = {}};
    }

    return std::unexpected("no usable metadata from '" + exe.string() + "' via --scrap-metadata or --help");
}

}  // namespace scrap::Command
