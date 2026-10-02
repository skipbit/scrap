#include "build/BuildSteps.h"

#include "build/BuildReporter.h"
#include "build/BuildStep.h"
#include "build/StepRunner.h"
#include "compile/ArchiveCommand.h"
#include "compile/CompileCommand.h"
#include "compile/LinkCommand.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <expected>  // IWYU pragma: keep
#include <mutex>
#include <optional>
#include <span>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace scrap::Build {

std::vector<BuildStep> buildSteps(const std::vector<Compile::CompileCommand>& compiles,
                                  const std::vector<Compile::ArchiveCommand>& archives,
                                  const std::vector<Compile::LinkCommand>& links)
{
    std::vector<BuildStep> steps;
    steps.reserve(compiles.size() + archives.size() + links.size());
    for (const Compile::CompileCommand& command : compiles) {
        steps.push_back(BuildStep{ .kind = StepKind::Compile, .target = command.target, .subject = command.file, .directory = command.directory, .output = command.output, .arguments = command.arguments });
    }
    for (const Compile::ArchiveCommand& command : archives) {
        steps.push_back(BuildStep{ .kind = StepKind::Archive, .target = command.target, .subject = command.output, .directory = command.directory, .output = command.output, .arguments = command.arguments });
    }
    for (const Compile::LinkCommand& command : links) {
        steps.push_back(BuildStep{ .kind = StepKind::Link, .target = command.target, .subject = command.output, .directory = command.directory, .output = command.output, .arguments = command.arguments });
    }
    return steps;
}

namespace {

/**
 * The position of @p kind in the order the kinds run in.
 */
constexpr std::size_t kindIndex(const StepKind kind)
{
    return static_cast<std::size_t>(kind);
}

/**
 * What the threads running a build share. Every member that changes is read
 * and written with the mutex held.
 */
class StepQueue {
public:
    StepQueue(const std::vector<BuildStep>& steps, StepRunner& runner, BuildReporter& reporter)
        : _steps(&steps)
        , _runner(&runner)
        , _reporter(&reporter)
    {
    }

    /**
     * Take steps and run them until there is none left to start. What a
     * step or the reporter throws ends the build as a failure does, and is
     * passed on by outcome().
     */
    void work()
    {
        std::unique_lock lock{ _mutex };
        while (const std::optional<std::size_t> index = take(lock)) {
            const BuildStep& step = (*_steps)[*index];
            try {
                _reporter->started(step);
                lock.unlock();
                StepResult result = _runner->run(step);
                lock.lock();
                _reporter->finished(step, result.output);
                if (result.failure.has_value()) {
                    _failures.push_back(FailedStep{ .step = step, .failure = *result.failure });
                }
            } catch (...) {
                if (! lock.owns_lock()) {
                    lock.lock();
                }
                if (! _exception) {
                    _exception = std::current_exception();
                }
            }
            --_running[kindIndex(step.kind)];
            _changed.notify_all();
        }
    }

    /**
     * What the build came to, once every thread has returned from work().
     */
    std::expected<void, std::vector<FailedStep>> outcome()
    {
        if (_exception) {
            std::rethrow_exception(_exception);
        }
        if (! _failures.empty()) {
            return std::unexpected(std::move(_failures));
        }
        return {};
    }

private:
    /**
     * Whether a step of a kind that runs before @p kind is still running.
     */
    [[nodiscard]] bool earlierKindRunning(const StepKind kind) const
    {
        return std::ranges::any_of(std::span{ _running }.first(kindIndex(kind)), [](const std::size_t count) {
            return count > 0;
        });
    }

    /**
     * The index of the next step to run, waiting while a step of a kind that
     * runs before it is still running; or nothing once no step is to start.
     */
    std::optional<std::size_t> take(std::unique_lock<std::mutex>& lock)
    {
        _changed.wait(lock, [this] {
            return stopped() || (_next == _steps->size()) || (! earlierKindRunning((*_steps)[_next].kind));
        });
        if (stopped() || (_next == _steps->size())) {
            return std::nullopt;
        }
        const std::size_t index = _next++;
        ++_running[kindIndex((*_steps)[index].kind)];
        return index;
    }

    [[nodiscard]] bool stopped() const
    {
        return (! _failures.empty()) || _exception;
    }

    const std::vector<BuildStep>* _steps;
    StepRunner* _runner;
    BuildReporter* _reporter;
    std::mutex _mutex;
    std::condition_variable _changed;
    std::size_t _next = 0;
    std::array<std::size_t, StepKindCount> _running{};  ///< Steps of each kind that have started and not ended.
    std::vector<FailedStep> _failures;
    std::exception_ptr _exception;
};

}  // anonymous namespace

std::expected<void, std::vector<FailedStep>> runSteps(const std::vector<BuildStep>& steps,
                                                      StepRunner& runner,
                                                      BuildReporter& reporter,
                                                      const unsigned parallelism)
{
    StepQueue queue{ steps, runner, reporter };
    const std::size_t threads = std::min<std::size_t>(std::max(parallelism, 1U), steps.size());

    // This thread is one of the threads that run steps. A thread the system
    // will not start leaves the build to fewer of them.
    std::vector<std::thread> helpers;
    helpers.reserve(threads);
    for (std::size_t count = 1; count < threads; ++count) {
        try {
            helpers.emplace_back([&queue] {
                queue.work();
            });
        } catch (const std::system_error&) {
            break;
        }
    }
    queue.work();
    for (std::thread& helper : helpers) {
        helper.join();
    }
    return queue.outcome();
}

}  // namespace scrap::Build
