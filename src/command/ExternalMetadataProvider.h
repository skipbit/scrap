#pragma once

#include "command/OptionSchema.h"

#include <expected>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Metadata returned by an external command's --scrap-metadata protocol.
 */
struct ExternalCommandMetadata {
    std::string name;
    std::string description;
    OptionSchema options;
};

/**
 * @brief Abstract interface for fetching metadata from external commands.
 *
 * Concrete implementations invoke the external executable with a
 * protocol flag (e.g. --scrap-metadata) and parse the response.
 */
class ExternalMetadataProvider {
public:
    virtual ~ExternalMetadataProvider();

    /**
     * @brief Fetch metadata from an external command executable.
     *
     * @param executable Path to the scrap-* executable.
     * @return Metadata on success, or an error message on failure.
     */
    [[nodiscard]] virtual auto fetch(const std::filesystem::path& executable)
        -> std::expected<ExternalCommandMetadata, std::string> = 0;
};

}  // namespace scrap::Command
