#include "command/ExternalCommandResolver.h"

#include "command/CommandEntry.h"
#include "command/CommandHandler.h"
#include "command/CommandSource.h"
#include "command/ExternalMetadataProvider.h"
#include "command/OptionSchema.h"
#include "command/ParsedOptions.h"
#include "command/RuntimeEnvironment.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::string_view ExternalPrefix = "scrap-";

/**
 * Check if a directory entry is an executable scrap-* command.
 */
auto isScrapExecutable(const std::filesystem::directory_entry& entry) -> bool
{
    if (! entry.is_regular_file()) {
        return false;
    }
    auto filename = entry.path().filename().string();
    if (! filename.starts_with(ExternalPrefix)) {
        return false;
    }
    auto status = std::filesystem::status(entry.path());
    return (status.permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
}

/**
 * Extract the command name from a scrap-* filename.
 */
auto commandNameFrom(const std::filesystem::path& path) -> std::string
{
    return path.filename().string().substr(ExternalPrefix.size());
}

/**
 * Build a CommandEntry from a discovered executable, fetching metadata if available.
 */
auto buildEntry(const std::filesystem::path& executablePath, ExternalMetadataProvider* provider) -> CommandEntry
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
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return nullptr;
    };
    return entry;
}

}  // anonymous namespace

/**
 * Construct with an ExternalMetadataProvider for fetching command metadata.
 */
ExternalCommandResolver::ExternalCommandResolver(std::unique_ptr<ExternalMetadataProvider> metadataProvider)
    : metadataProvider_(std::move(metadataProvider))
{
}

/**
 * Scan env.searchPaths for scrap-* executables and build CommandEntry list.
 */
auto ExternalCommandResolver::resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry>
{
    std::vector<CommandEntry> entries;

    for (const auto& searchPath : env.searchPaths) {
        if (! std::filesystem::is_directory(searchPath)) {
            continue;
        }
        for (const auto& dirEntry : std::filesystem::directory_iterator(searchPath)) {
            if (isScrapExecutable(dirEntry)) {
                entries.push_back(buildEntry(dirEntry.path(), metadataProvider_.get()));
            }
        }
    }

    return entries;
}

}  // namespace scrap::Command
