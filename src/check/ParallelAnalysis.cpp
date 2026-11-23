#include "check/ParallelAnalysis.h"

#include "tool/ExternalTool.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Check {

AnalysisReporter::~AnalysisReporter() = default;

namespace {

/// What the tool wrote for @p result, whether or not it succeeded.
std::string_view outputOf(const FileResult& result)
{
    if (result.has_value()) {
        return *result;
    }
    if (const auto* failed = std::get_if<Tool::ToolFailed>(&result.error())) {
        return failed->output;
    }
    return {};
}

/**
 * What the threads analyzing the files share. Every member that changes is
 * read and written with the mutex held.
 */
class FileQueue {
public:
    FileQueue(const std::vector<std::filesystem::path>& files, const FileAnalyzer& analyze, AnalysisReporter& reporter)
        : _files(&files)
        , _analyze(&analyze)
        , _reporter(&reporter)
    {
    }

    /**
     * Take files and analyze them until there is none left to start.
     */
    void work()
    {
        std::unique_lock lock{ _mutex };
        while (const std::optional<std::size_t> index = take()) {
            try {
                analyze((*_files)[*index], lock);
            } catch (...) {
                if (! lock.owns_lock()) {
                    lock.lock();
                }
                if (! _exception) {
                    _exception = std::current_exception();
                }
            }
        }
    }

    /**
     * What the analysis came to, once every thread has returned from work().
     */
    std::vector<FailedAnalysis> outcome()
    {
        if (_exception) {
            std::rethrow_exception(_exception);
        }
        return std::move(_failures);
    }

private:
    /**
     * Analyze @p file, reporting it as it starts and once it has ended. The
     * lock is held on entry and on return, and released while the tool runs.
     */
    void analyze(const std::filesystem::path& file, std::unique_lock<std::mutex>& lock)
    {
        _reporter->started(file);
        lock.unlock();
        FileResult result = (*_analyze)(file);
        lock.lock();
        _reporter->finished(file, outputOf(result));
        if (! result.has_value()) {
            _notRun = _notRun || std::holds_alternative<Tool::ToolNotRun>(result.error());
            _failures.push_back(FailedAnalysis{ .file = file, .failure = std::move(result.error()) });
        }
    }

    /**
     * The index of the next file to analyze, or nothing once no file is to
     * start.
     */
    std::optional<std::size_t> take()
    {
        if (_notRun || _exception || (_next == _files->size())) {
            return std::nullopt;
        }
        return _next++;
    }

    const std::vector<std::filesystem::path>* _files;
    const FileAnalyzer* _analyze;
    AnalysisReporter* _reporter;
    std::mutex _mutex;
    std::size_t _next = 0;
    bool _notRun = false;  ///< Whether the tool could not be started for a file.
    std::vector<FailedAnalysis> _failures;
    std::exception_ptr _exception;
};

}  // anonymous namespace

std::vector<FailedAnalysis> analyzeFiles(const std::vector<std::filesystem::path>& files,
                                         const FileAnalyzer& analyze,
                                         AnalysisReporter& reporter,
                                         const unsigned parallelism)
{
    FileQueue queue{ files, analyze, reporter };
    const std::size_t threads = std::min<std::size_t>(std::max(parallelism, 1U), files.size());

    // This thread is one of the threads that analyze files. A thread the
    // system will not start leaves the analysis to fewer of them.
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

}  // namespace scrap::Check
