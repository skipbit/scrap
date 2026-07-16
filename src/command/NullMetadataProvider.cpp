#include "command/NullMetadataProvider.h"

#include "command/ExternalMetadataProvider.h"

#include <expected>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * Always return an error indicating the metadata protocol is not yet implemented.
 */
// NOLINTNEXTLINE(readability-convert-member-functions-to-static) — virtual override
auto NullMetadataProvider::fetch([[maybe_unused]] const std::filesystem::path& executable)
    -> std::expected<ExternalCommandMetadata, std::string>
{
    return std::unexpected(std::string{"external metadata protocol not yet implemented"});
}

}  // namespace scrap::Command
