#pragma once

#include "toolchain/HostEnvironment.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief A toolchain installed below SCRAP_HOME for the host.
 */
struct InstalledToolchain {
    std::string family;
    ToolchainVersion version;
    std::filesystem::path directory;                               ///< <SCRAP_HOME>/toolchains/<family>/<version>/<platform>-<arch>
    std::filesystem::path compiler;                                ///< The C++ compiler inside the directory.
    std::vector<std::filesystem::path> runtimeLibraryDirectories;  ///< Where its runtime libraries are inside the directory.
};

/**
 * @brief The newest toolchain installed below @p scrapHome for @p host that
 *        @p specifier names.
 *
 * A toolchain counts as installed when its directory holds an install
 * record. A directory whose name is not a version is passed over. When the
 * newest one's record cannot be read, or names a compiler that is not
 * there, or a runtime library directory that is not there, that is an
 * error rather than a reason to choose an older one.
 *
 * @return The toolchain, nothing when none is installed, or why the one
 *         found cannot be used.
 */
[[nodiscard]] std::expected<std::optional<InstalledToolchain>, std::string> findInstalledToolchain(const std::filesystem::path& scrapHome,
                                                                                                   const HostEnvironment& host,
                                                                                                   const ToolchainSpecifier& specifier);

/**
 * @brief One toolchain installed below @p scrapHome for @p host, to show as
 *        an example: the newest of the family that comes first by name.
 *
 * A family whose newest toolchain cannot be used is passed over.
 *
 * @return The toolchain, or nothing when none is installed.
 */
[[nodiscard]] std::optional<InstalledToolchain> anyInstalledToolchain(const std::filesystem::path& scrapHome, const HostEnvironment& host);

}  // namespace scrap::Toolchain
