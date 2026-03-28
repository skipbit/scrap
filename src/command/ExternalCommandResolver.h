#pragma once

#include "command/CommandResolver.h"
#include "command/ExternalMetadataProvider.h"

#include <memory>

namespace scrap::Command {

/**
 * @brief Resolver that discovers external commands from the filesystem.
 *
 * Scans directories in RuntimeEnvironment::searchPaths for executables
 * matching the scrap-* naming pattern and builds CommandEntry trees
 * with metadata fetched from ExternalMetadataProvider.
 */
class ExternalCommandResolver : public CommandResolver {
public:
    /**
     * @brief Construct with a metadata provider for fetching command info.
     *
     * @param metadataProvider Provider that fetches metadata from external executables.
     */
    explicit ExternalCommandResolver(std::unique_ptr<ExternalMetadataProvider> metadataProvider);

    /**
     * @brief Scan search paths for scrap-* executables and return entries.
     *
     * @param env Runtime environment containing searchPaths to scan.
     * @return CommandEntry list for discovered external commands.
     */
    auto resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry> override;

private:
    std::unique_ptr<ExternalMetadataProvider> metadataProvider_;
};

}  // namespace scrap::Command
