#include "command/ExternalCommandResolver.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

namespace {

const std::string kExternalPrefix = "scrap-";

/**
 * @brief Check if a directory entry is an executable scrap-* command.
 */
auto isScrapExecutable(const std::filesystem::directory_entry& entry) -> bool
{
    if (!entry.is_regular_file()) {
        return false;
    }
    auto filename = entry.path().filename().string();
    if (!filename.starts_with(kExternalPrefix)) {
        return false;
    }
    auto status = std::filesystem::status(entry.path());
    return (status.permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
}

/**
 * @brief Extract the command name from a scrap-* filename.
 */
auto commandNameFrom(const std::filesystem::path& path) -> std::string
{
    return path.filename().string().substr(kExternalPrefix.size());
}

}  // anonymous namespace

ExternalCommandResolver::ExternalCommandResolver(std::unique_ptr<ExternalMetadataProvider> metadataProvider)
    : metadataProvider_(std::move(metadataProvider))
{
}

auto ExternalCommandResolver::resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry>
{
    std::vector<CommandEntry> entries;

    for (const auto& searchPath : env.searchPaths) {
        if (!std::filesystem::is_directory(searchPath)) {
            continue;
        }

        for (const auto& dirEntry : std::filesystem::directory_iterator(searchPath)) {
            if (!isScrapExecutable(dirEntry)) {
                continue;
            }

            auto name = commandNameFrom(dirEntry.path());
            std::string description;
            OptionSchema options;

            // Attempt metadata fetch; fall back to empty description on failure.
            if (metadataProvider_) {
                auto metadata = metadataProvider_->fetch(dirEntry.path());
                if (metadata.has_value()) {
                    description = std::move(metadata->description);
                    options = std::move(metadata->options);
                    if (!metadata->name.empty()) {
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
            auto execPath = dirEntry.path();
            entry.createHandler = [execPath](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
                // External command execution will be implemented later.
                return nullptr;
            };
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

}  // namespace scrap::Command
