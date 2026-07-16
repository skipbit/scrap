#pragma once

#include "command/ExternalMetadataProvider.h"

#include <expected>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Metadata provider stub used until the external metadata protocol lands.
 *
 * External commands are still discovered by ExternalCommandResolver via
 * filesystem scanning; this provider simply reports that fetching rich
 * metadata (name/description/options) is not yet supported. The real
 * --scrap-metadata protocol is implemented in a later phase.
 */
class NullMetadataProvider final : public ExternalMetadataProvider {
public:
    /**
     * @brief Always report metadata fetching as unimplemented.
     *
     * @param executable Path to the scrap-* executable (unused).
     * @return An error describing that the protocol is not yet implemented.
     */
    [[nodiscard]] auto
    fetch(const std::filesystem::path& executable) -> std::expected<ExternalCommandMetadata, std::string> override;
};

}  // namespace scrap::Command
