#include "command/ExternalCommandResolver.h"

#include "command/CommandEntry.h"
#include "command/CommandHandler.h"
#include "command/CommandSource.h"
#include "command/ExternalMetadataProvider.h"
#include "command/OptionSchema.h"
#include "command/ParsedOptions.h"
#include "command/RuntimeEnvironment.h"
#include "process/ExecutableFile.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::string_view ExternalPrefix = "scrap-";

/**
 * Check if a directory entry is an executable scrap-* command.
 */
bool isScrapExecutable(const std::filesystem::directory_entry& entry)
{
    return entry.path().filename().string().starts_with(ExternalPrefix) && Process::isExecutableFile(entry.path());
}

/**
 * Extract the command name from a scrap-* filename.
 */
std::string commandNameFrom(const std::filesystem::path& path)
{
    return path.filename().string().substr(ExternalPrefix.size());
}

/**
 * Build a CommandEntry from a discovered executable, fetching metadata if available.
 */
CommandEntry buildEntry(const std::filesystem::path& executablePath, ExternalMetadataProvider* provider)
{
    auto name = commandNameFrom(executablePath);
    std::string description;
    OptionSchema options;

    if (provider != nullptr) {
        auto metadata = provider->fetch(executablePath);
        if (metadata.has_value()) {
            description = std::move(metadata->description);
            options = std::move(metadata->options);
            if (! metadata->name.empty()) {
                name = std::move(metadata->name);
            }
        }
    }

    CommandEntry entry;
    entry.spec.name = name;
    entry.spec.description = description;
    entry.spec.category = "External Commands";
    entry.spec.options = std::move(options);
    entry.source = CommandSource::External;
    entry.location = executablePath;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return nullptr;
    };
    return entry;
}

/**
 * @p directory spelled so that two spellings of one directory compare equal:
 * normalized, without a trailing separator.
 */
std::filesystem::path comparableDirectory(const std::filesystem::path& directory)
{
    std::filesystem::path normal = directory.lexically_normal();
    if ((! normal.has_filename()) && (normal != normal.root_path())) {
        normal = normal.parent_path();
    }
    return normal;
}

/**
 * Where external commands are looked for, in order: the directory below
 * SCRAP_HOME that scrap installs commands into, then PATH. A directory listed
 * again is left out, so each command in it is found once.
 */
std::vector<std::filesystem::path> commandSearchPaths(const RuntimeEnvironment& env)
{
    std::vector<std::filesystem::path> candidates;
    if (env.scrapHome.has_value()) {
        candidates.push_back(*env.scrapHome / "bin");
    }
    candidates.insert(candidates.end(), env.systemSearchPaths.begin(), env.systemSearchPaths.end());

    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::path& candidate : candidates) {
        std::filesystem::path directory = comparableDirectory(candidate);
        if (! std::ranges::contains(paths, directory)) {
            paths.push_back(std::move(directory));
        }
    }
    return paths;
}

}  // anonymous namespace

/**
 * Construct with an ExternalMetadataProvider for fetching command metadata.
 */
ExternalCommandResolver::ExternalCommandResolver(std::unique_ptr<ExternalMetadataProvider> metadataProvider)
    : _metadataProvider(std::move(metadataProvider))
{
}

/**
 * Scan the command search paths for scrap-* executables and build CommandEntry list.
 */
std::vector<CommandEntry> ExternalCommandResolver::resolve(const RuntimeEnvironment& env)
{
    std::vector<CommandEntry> entries;

    for (const auto& searchPath : commandSearchPaths(env)) {
        std::error_code ec;
        if ((! std::filesystem::is_directory(searchPath, ec)) || ec) {
            continue;
        }
        for (std::filesystem::directory_iterator it(searchPath, ec), end; (! ec) && (it != end); it.increment(ec)) {
            if (isScrapExecutable(*it)) {
                entries.push_back(buildEntry(it->path(), _metadataProvider.get()));
            }
        }
    }

    return entries;
}

}  // namespace scrap::Command
