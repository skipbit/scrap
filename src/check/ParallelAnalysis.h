#pragma once

#include "tool/ExternalTool.h"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Check {

/**
 * @brief What analyzing one file came to: what the tool wrote, or why it
 *        failed.
 */
using FileResult = std::expected<std::string, Tool::ToolFailure>;

/**
 * @brief Analyzes one file. Called from several threads at once, each for a
 *        file of its own.
 */
using FileAnalyzer = std::function<FileResult(const std::filesystem::path& file)>;

/**
 * @brief Told of each file as it is analyzed.
 *
 * Calls come one at a time, from whichever thread analyzed the file.
 */
class AnalysisReporter {
public:
    virtual ~AnalysisReporter();
    AnalysisReporter(const AnalysisReporter&) = default;
    AnalysisReporter& operator=(const AnalysisReporter&) = default;
    AnalysisReporter(AnalysisReporter&&) = default;
    AnalysisReporter& operator=(AnalysisReporter&&) = default;

    /**
     * @brief @p file is about to be analyzed.
     */
    virtual void started(const std::filesystem::path& file) = 0;

    /**
     * @brief @p file has been analyzed, whether or not that succeeded.
     *
     * @param file The file analyzed.
     * @param output What the tool wrote, which may be empty.
     */
    virtual void finished(const std::filesystem::path& file, std::string_view output) = 0;

protected:
    AnalysisReporter() = default;
};

/**
 * @brief A file whose analysis failed, and why.
 */
struct FailedAnalysis {
    std::filesystem::path file;
    Tool::ToolFailure failure;
};

/**
 * @brief Analyze @p files, as many at once as @p parallelism allows.
 *
 * Files are taken in the order given. Every file is analyzed whatever the
 * others came to, since each is analyzed on its own; only a tool that cannot
 * be started stops the analysis, as it would fail every file the same way.
 * No file starts after that, and the files already being analyzed are waited
 * for.
 *
 * The reporter is told of one file at a time, never from two threads
 * together: of each file as it starts, and again once it has ended, with
 * what the tool wrote, so what the files wrote reaches it in the order they
 * ended. What @p analyze or the reporter throws stops the analysis as a tool
 * that cannot be started does, and reaches the caller; a file whose analysis
 * threw is not reported as ended.
 *
 * @param files The files to analyze, in order.
 * @param analyze Analyzes each file.
 * @param reporter Told of each file as it is analyzed.
 * @param parallelism How many files may be analyzed at once. 0 is taken as 1.
 * @return The files whose analysis failed, in the order they ended; empty
 *         when every file succeeded.
 */
[[nodiscard]] std::vector<FailedAnalysis> analyzeFiles(const std::vector<std::filesystem::path>& files,
                                                       const FileAnalyzer& analyze,
                                                       AnalysisReporter& reporter,
                                                       unsigned parallelism);

}  // namespace scrap::Check
