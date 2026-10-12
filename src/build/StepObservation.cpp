#include "build/StepObservation.h"

#include "build/BuildStep.h"
#include "build/StepRecord.h"
#include "compile/DependencyFile.h"
#include "file/WholeFile.h"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <set>
#include <sys/stat.h>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Build {

namespace {

/// Nanoseconds in a second.
constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;

/**
 * Add the stamp of each of @p paths to @p stamps, or report that one of them
 * cannot be examined.
 */
bool addStamps(std::vector<InputStamp>& stamps, const std::vector<std::filesystem::path>& paths, const std::filesystem::path& directory)
{
    for (const std::filesystem::path& path : paths) {
        auto stamp = stampOf(path, directory);
        if (! stamp.has_value()) {
            return false;
        }
        stamps.push_back(std::move(*stamp));
    }
    return true;
}

/**
 * The files the dependency file of @p step lists, or nothing when it cannot
 * be read.
 */
std::optional<std::vector<std::filesystem::path>> listedFiles(const BuildStep& step)
{
    if (! step.dependencyFile.has_value()) {
        return std::nullopt;
    }
    const auto text = File::readWholeFile(step.directory / *step.dependencyFile);
    if (! text.has_value()) {
        return std::nullopt;
    }
    return Compile::parseDependencyFile(*text);
}

/**
 * @p path below @p place, or nothing when it does not lie below it.
 */
std::optional<std::filesystem::path> below(const std::filesystem::path& path, const std::filesystem::path& place)
{
    std::filesystem::path relative = path.lexically_relative(place);
    if (relative.empty() || (*relative.begin() == "..")) {
        return std::nullopt;
    }
    return relative;
}

/**
 * The places the compiler may look for a header of @p step that reads
 * @p files: the directory of the source, the directories named for headers,
 * and the directory of each file read that is named relative to the step.
 */
std::set<std::filesystem::path> placesSearched(const BuildStep& step, const std::vector<std::filesystem::path>& files)
{
    std::set<std::filesystem::path> places;
    const auto add = [&places](const std::filesystem::path& place) {
        places.insert(place.empty() ? std::filesystem::path{ "." } : place.lexically_normal());
    };
    add(step.subject.parent_path());
    for (const std::filesystem::path& directory : step.searchedDirectories) {
        add(directory);
    }
    for (const std::filesystem::path& file : files) {
        if (file.is_relative()) {
            add(file.parent_path());
        }
    }
    return places;
}

/**
 * Add to @p paths each path below @p places, other than @p file, where a file
 * named @p name would be found.
 */
void addShadowing(std::set<std::filesystem::path>& paths,
                  const std::filesystem::path& file,
                  const std::filesystem::path& name,
                  const std::set<std::filesystem::path>& places)
{
    for (const std::filesystem::path& place : places) {
        std::filesystem::path candidate = (place / name).lexically_normal();
        if (candidate != file) {
            paths.insert(std::move(candidate));
        }
    }
}

/**
 * Those of @p paths, named relative to @p directory, where nothing is.
 */
std::vector<std::filesystem::path> absentPaths(const std::vector<std::filesystem::path>& paths,
                                               const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> absent;
    for (const std::filesystem::path& path : paths) {
        std::error_code ec;
        if ((! std::filesystem::exists(directory / path, ec)) && (! ec)) {
            absent.push_back(path);
        }
    }
    return absent;
}

}  // anonymous namespace

std::optional<InputStamp> stampOf(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    const std::filesystem::path file = directory / path;
    struct stat status { };
    if (::stat(file.c_str(), &status) != 0) {
        return std::nullopt;
    }
#if defined(__APPLE__)
    const timespec& modified = status.st_mtimespec;
#else
    const timespec& modified = status.st_mtim;
#endif
    return InputStamp{ .path = path,
                       .modified = (static_cast<std::int64_t>(modified.tv_sec) * NanosecondsPerSecond) + modified.tv_nsec,
                       .size = static_cast<std::uintmax_t>(status.st_size) };
}

std::vector<std::filesystem::path> shadowingPaths(const std::set<std::filesystem::path>& places,
                                                  const std::vector<std::filesystem::path>& files)
{
    std::set<std::filesystem::path> paths;
    for (const std::filesystem::path& listed : files) {
        const std::filesystem::path file = listed.lexically_normal();
        for (const std::filesystem::path& place : places) {
            if (const auto name = below(file, place); name.has_value()) {
                addShadowing(paths, file, *name, places);
            }
        }
    }
    return { paths.begin(), paths.end() };
}

std::optional<StepRecord> observeStep(const BuildStep& step)
{
    if (step.arguments.empty()) {
        return std::nullopt;
    }
    StepRecord record{ .arguments = step.arguments, .inputs = {}, .absent = {} };
    if (! addStamps(record.inputs, { step.arguments.front() }, step.directory)) {
        return std::nullopt;
    }
    if (! addStamps(record.inputs, step.inputs, step.directory)) {
        return std::nullopt;
    }
    if (step.kind != StepKind::Compile) {
        return record;
    }

    const auto files = listedFiles(step);
    if ((! files.has_value()) || (! addStamps(record.inputs, *files, step.directory))) {
        return std::nullopt;
    }
    record.absent = absentPaths(shadowingPaths(placesSearched(step, *files), *files), step.directory);
    return record;
}

}  // namespace scrap::Build
