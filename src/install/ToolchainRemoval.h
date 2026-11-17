#pragma once

#include <expected>
#include <filesystem>
#include <system_error>

namespace scrap::Install {

/**
 * @brief An installed toolchain that could not be taken away.
 */
struct ToolchainNotRemoved {
    std::filesystem::path path;
    std::error_code code;
};

/**
 * @brief Take away the toolchain installed at @p directory below
 *        @p scrapHome.
 *
 * The directory is first moved below the work directory, so it is gone
 * from where toolchains are found at once and nothing half removed is left
 * there. What the move leaves empty of the version and the family is
 * removed too. What cannot be deleted below the work directory is left for
 * a later install to discard.
 *
 * @return Nothing, or what could not be moved away and why.
 */
[[nodiscard]] std::expected<void, ToolchainNotRemoved> removeInstalledToolchain(const std::filesystem::path& scrapHome,
                                                                                const std::filesystem::path& directory);

}  // namespace scrap::Install
