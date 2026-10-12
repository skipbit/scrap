#include "build/StepHistory.h"

#include "build/BuildStep.h"
#include "build/StepObservation.h"
#include "build/StepRecord.h"
#include "file/WholeFile.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace scrap::Build {

namespace {

/// Directory below the build directory that holds the records of the steps.
constexpr std::string_view RecordDirectory = "records";

}  // anonymous namespace

StepHistory::~StepHistory() = default;

RecordedStepHistory::RecordedStepHistory(std::filesystem::path buildDirectory)
    : _buildDirectory(std::move(buildDirectory))
{
}

std::optional<std::filesystem::path> RecordedStepHistory::recordFile(const BuildStep& step) const
{
    const std::filesystem::path relative = step.output.lexically_relative(_buildDirectory);
    if (relative.empty() || (*relative.begin() == "..") || (*relative.begin() == RecordDirectory)) {
        return std::nullopt;
    }
    return step.directory / _buildDirectory / RecordDirectory / relative;
}

bool RecordedStepHistory::upToDate(const BuildStep& step)
{
    const auto file = recordFile(step);
    std::error_code ec;
    if ((! file.has_value()) || (! std::filesystem::exists(step.directory / step.output, ec))) {
        return false;
    }
    const auto text = File::readWholeFile(*file);
    if (! text.has_value()) {
        return false;
    }
    const auto recorded = parseStepRecord(*text);
    if (! recorded.has_value()) {
        return false;
    }
    const auto observed = observeStep(step);
    return observed.has_value() && (*observed == *recorded);
}

void RecordedStepHistory::starting(const BuildStep& step)
{
    if (const auto file = recordFile(step); file.has_value()) {
        std::error_code ec;
        std::filesystem::remove(*file, ec);
    }
}

void RecordedStepHistory::succeeded(const BuildStep& step)
{
    const auto file = recordFile(step);
    const auto observed = observeStep(step);
    if ((! file.has_value()) || (! observed.has_value())) {
        return;
    }
    // A record that cannot be written leaves the step to run next time.
    std::error_code ec;
    std::filesystem::create_directories(file->parent_path(), ec);
    if (! ec) {
        const std::error_code written = File::replaceFile(*file, renderStepRecord(*observed));
        static_cast<void>(written);
    }
}

}  // namespace scrap::Build
