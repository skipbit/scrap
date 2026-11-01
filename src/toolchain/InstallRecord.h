#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

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
    std::filesystem::path compiler;  ///< The C++ compiler, relative to the toolchain's directory.
};

/**
 * @brief Read the text of an install record.
 *
 * Keys this version does not know are passed over. The compiler must be a
 * relative path that stays inside the toolchain's directory.
 *
 * @return The record, or where and why it cannot be read.
 */
[[nodiscard]] std::expected<InstallRecord, std::string> parseInstallRecord(std::string_view text);

}  // namespace scrap::Toolchain
