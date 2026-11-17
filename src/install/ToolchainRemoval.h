#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

namespace scrap::Install {

/**
 * @brief An installed toolchain that could not be taken away.
 */
struct ToolchainNotRemoved {
    std::filesystem::path path;
    std::error_code code;
    /// The work directory that could not be made, when that is why.
    std::optional<std::filesystem::path> workDirectory = std::nullopt;
};

/**
 * @brief An installed toolchain taken away.
 */
struct ToolchainRemoved {
    /// Where what could not be deleted is left, when something is.
    std::optional<std::filesystem::path> leftAt = std::nullopt;
};

/**
 * @brief Take away the toolchain installed at @p directory below
 *        @p scrapHome.
 *
 * The directory is first moved below the work directory, so it is gone
 * from where toolchains are found at once and nothing half removed is left
 * there. What the move leaves empty of the version and the family is
 * removed too. What cannot be deleted is left below the work directory,
 * and where is told.
 *
 * @return What was left behind, or what could not be moved away and why.
 */
[[nodiscard]] std::expected<ToolchainRemoved, ToolchainNotRemoved> removeInstalledToolchain(const std::filesystem::path& scrapHome,
                                                                                            const std::filesystem::path& directory);

}  // namespace scrap::Install
