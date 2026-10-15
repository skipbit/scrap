#include "build/StepObservation.h"

#include "build/BuildStep.h"
#include "build/StepRecord.h"
#include "compile/DependencyFile.h"
#include "compile/HeaderSearch.h"
#include "compile/IncludeDirectives.h"
#include "file/WholeFile.h"

#include <algorithm>
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
 * Whether @p file, read by @p step, belongs to the project: it is named
 * relative to the step, or lies below a directory the step names for
 * headers. The others are the system's.
 */
bool namesItsHeaders(const BuildStep& step, const std::filesystem::path& file)
{
    return file.is_relative() || std::ranges::any_of(step.searchedDirectories, [&file](const std::filesystem::path& directory) {
        return below(file.lexically_normal(), directory.lexically_normal()).has_value();
    });
}

/**
 * The searches the compiler makes for the headers the files of @p step among
 * @p files name and, when a file names a header by a macro, one more search,
 * which no file ends, of every path a header could appear at. Nothing is
 * returned when a file cannot be read.
 */
std::optional<std::vector<Compile::HeaderSearch>> searchesOf(const BuildStep& step, const std::vector<std::filesystem::path>& files)
{
    std::vector<Compile::HeaderSearch> searches;
    bool computed = false;
    for (const std::filesystem::path& file : files) {
        if (! namesItsHeaders(step, file)) {
            continue;
        }
        const auto text = File::readWholeFile(step.directory / file);
        if (! text.has_value()) {
            return std::nullopt;
        }
        const Compile::IncludeDirectives directives = Compile::parseIncludeDirectives(*text);
        computed = computed || directives.computed;
        for (const Compile::IncludedName& included : directives.names) {
            searches.push_back(Compile::headerSearch(included, file.parent_path(), step.searchedDirectories));
        }
    }
    if (computed) {
        searches.push_back(Compile::HeaderSearch{ .paths = shadowingPaths(placesSearched(step, files), files), .stopsAtAFile = false });
    }
    return searches;
}

/**
 * The first of the directories above @p path that is a directory, named
 * relative to @p directory like @p path.
 */
std::filesystem::path nearestDirectoryThatIsThere(const std::filesystem::path& path, const std::filesystem::path& directory)
{
    const std::vector<std::filesystem::path> above = directoriesAbove(path);
    const auto there = std::ranges::find_if(above, [&directory](const std::filesystem::path& candidate) {
        std::error_code ec;
        return std::filesystem::is_directory(directory / candidate, ec);
    });
    return (there == above.end()) ? above.back() : *there;
}

/**
 * Add to @p absent the paths of @p search, named relative to @p directory,
 * where no file is, up to the first that holds one when that ends it. A
 * directory is passed over, as the compiler passes it over, and listed, as a
 * file may take its place.
 */
void addAbsent(std::set<std::filesystem::path>& absent, const Compile::HeaderSearch& search, const std::filesystem::path& directory)
{
    for (const std::filesystem::path& path : search.paths) {
        std::error_code ec;
        const std::filesystem::file_type type = std::filesystem::status(directory / path, ec).type();
        if ((type == std::filesystem::file_type::not_found) || (type == std::filesystem::file_type::directory)) {
            absent.insert(path);
        } else if ((! ec) && search.stopsAtAFile) {
            return;
        }
    }
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

std::vector<std::filesystem::path> directoriesAbove(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> directories;
    for (std::filesystem::path directory = path.parent_path();; directory = directory.parent_path()) {
        directories.push_back(directory.empty() ? std::filesystem::path{ "." } : directory);
        if (directory.empty() || (directory == directory.parent_path())) {
            break;
        }
    }
    return directories;
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
    const auto searches = searchesOf(step, *files);
    if (! searches.has_value()) {
        return std::nullopt;
    }
    std::set<std::filesystem::path> absent;
    for (const Compile::HeaderSearch& search : *searches) {
        addAbsent(absent, search, step.directory);
    }
    record.absent.assign(absent.begin(), absent.end());
    return record;
}

std::optional<std::vector<std::filesystem::path>> watchedDirectories(const BuildStep& step)
{
    if (step.kind != StepKind::Compile) {
        return std::vector<std::filesystem::path>{};
    }
    const auto files = listedFiles(step);
    if (! files.has_value()) {
        return std::nullopt;
    }
    const auto searches = searchesOf(step, *files);
    if (! searches.has_value()) {
        return std::nullopt;
    }
    std::set<std::filesystem::path> watched;
    for (const Compile::HeaderSearch& search : *searches) {
        for (const std::filesystem::path& path : search.paths) {
            watched.insert(nearestDirectoryThatIsThere(path, step.directory));
        }
    }
    return std::vector<std::filesystem::path>{ watched.begin(), watched.end() };
}

}  // namespace scrap::Build
