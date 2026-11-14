#pragma once

#include "toolchain/ToolchainArguments.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief The name of the record scrap writes into a toolchain it installs.
 *
 * The record is written last, so a directory without one was not installed
 * to the end. It belongs to scrap, not to the toolchain, and its shape is
 * no contract with anyone else.
 */
inline constexpr std::string_view InstallRecordName = ".scrap-toolchain.toml";

/**
 * @brief What scrap recorded when it installed a toolchain.
 */
struct InstallRecord {
    std::filesystem::path compiler;                                 ///< The C++ compiler, relative to the toolchain's directory.
    std::vector<std::filesystem::path> runtimeLibraryDirectories;   ///< Where its runtime libraries are, relative to the same.
    ToolchainArguments arguments;                                   ///< What the compiler is given to compile and link with the toolchain.
    std::optional<std::string> ingotIntegrity = std::nullopt;       ///< The digest or SHA-256 the index gave for what was installed.
    std::optional<std::filesystem::path> cCompiler = std::nullopt;  ///< The C compiler, relative to the toolchain's directory.
    std::optional<std::string> source = std::nullopt;               ///< Where the ingot was fetched from: a registry reference or a URL.
    std::optional<std::string> installedAt = std::nullopt;          ///< When it was installed, in UTC, as 2026-11-12T11:37:11Z.
};

/**
 * @brief Read the text of an install record.
 *
 * Keys this version does not know are passed over. The compiler, the C
 * compiler and each of the runtime library directories must be a relative
 * path that stays inside the toolchain's directory. Compile and link
 * arguments are lists of strings. A record without runtime library
 * directories, or without compile or link arguments, names none, and one
 * without an integrity value cannot be compared with the index. A record
 * an earlier scrap wrote holds no C compiler, source or time of installing.
 *
 * @return The record, or where and why it cannot be read.
 */
[[nodiscard]] std::expected<InstallRecord, std::string> parseInstallRecord(std::string_view text);

/**
 * @brief The text of an install record that holds @p record.
 */
[[nodiscard]] std::string renderInstallRecord(const InstallRecord& record);

}  // namespace scrap::Toolchain
