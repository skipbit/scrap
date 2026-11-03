#pragma once

#include "toolchain/HostEnvironment.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <expected>  // IWYU pragma: keep
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief What an ingot must suit to be installed here.
 */
struct InstallingHost {
    HostEnvironment environment;
    ToolchainVersion scrapVersion;
    std::optional<ToolchainVersion> glibcVersion;  ///< Nothing when the host has no glibc to compare.
};

/**
 * @brief The toolchain chosen from an index, with the ingot to install.
 */
struct SelectedIngot {
    std::string family;
    ToolchainVersion version;
    std::string compiler;
    Ingot ingot;
};

/**
 * @brief The index holds nothing the specifier names.
 */
struct NoMatchInIndex {
    std::vector<std::string> versions;  ///< Versions of the family on offer, newest first.
    std::vector<std::string> families;  ///< Every family in the index, by name.
};

/**
 * @brief What the specifier names has no ingot for the host.
 */
struct NoIngotForHost {
    std::vector<std::string> versions;  ///< Versions of the family that have one, newest first.
};

/**
 * @brief What the specifier names is no longer offered.
 */
struct NoLongerOffered {
    std::string family;
    std::optional<std::string> alternative;  ///< The newest version that can be installed here.
};

/**
 * @brief The ingot needs a newer glibc than the host has.
 */
struct NeedsNewerGlibc {
    std::string required;
    std::string host;
};

/**
 * @brief The toolchain needs a newer scrap than the one running.
 */
struct NeedsNewerScrap {
    std::string required;
};

using IngotSelectionFault = std::variant<NoMatchInIndex, NoIngotForHost, NoLongerOffered, NeedsNewerGlibc, NeedsNewerScrap>;

/**
 * @brief The value the index gives to check @p ingot by: its digest or SHA-256.
 */
[[nodiscard]] std::string integrityOf(const Ingot& ingot);

/**
 * @brief Choose what to install for @p specifier: the newest version it names
 *        that can be installed on @p host.
 *
 * When it names versions and none can be installed, the fault is the one that
 * stops the newest of them.
 *
 * @return The toolchain and ingot, or why there is none.
 */
[[nodiscard]] std::expected<SelectedIngot, IngotSelectionFault> selectIngot(const ToolchainIndex& index, const ToolchainSpecifier& specifier, const InstallingHost& host);

}  // namespace scrap::Toolchain
