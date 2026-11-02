#pragma once

#include "toolchain/HostEnvironment.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
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

    /** The name that selects it, as <family>@<version>. */
    [[nodiscard]] std::string name() const;
};

/**
 * @brief No toolchain is built for the host scrap runs on.
 */
struct NoToolchainForHost { };

/**
 * @brief The installed toolchain that was found cannot be used.
 */
struct UnusableInstalled {
    std::string reason;
};

/**
 * @brief No toolchain the specifier names is installed.
 */
struct NotInstalled { };

/**
 * @brief The directory the versions of a family are kept in cannot be
 *        listed.
 */
struct CannotListToolchains {
    std::filesystem::path directory;
    std::error_code code;
};

/**
 * @brief Why no installed toolchain can be used for a specifier.
 */
using InstalledToolchainFault = std::variant<NoToolchainForHost, UnusableInstalled, NotInstalled, CannotListToolchains>;

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
 *         found cannot be used (UnusableInstalled) or the versions cannot
 *         be listed (CannotListToolchains).
 */
[[nodiscard]] std::expected<std::optional<InstalledToolchain>, InstalledToolchainFault> findInstalledToolchain(const std::filesystem::path& scrapHome,
                                                                                                               const HostEnvironment& host,
                                                                                                               const ToolchainSpecifier& specifier);

/**
 * @brief The newest toolchain installed below @p scrapHome for the host
 *        scrap runs on that @p specifier names.
 *
 * @return The toolchain, or why none can be used.
 */
[[nodiscard]] std::expected<InstalledToolchain, InstalledToolchainFault> findInstalledToolchainForHost(const std::filesystem::path& scrapHome,
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
