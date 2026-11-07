#pragma once

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
    std::filesystem::path compiler;                                ///< The C++ compiler, relative to the toolchain's directory.
    std::vector<std::filesystem::path> runtimeLibraryDirectories;  ///< Where its runtime libraries are, relative to the same.
    std::vector<std::string> compileArguments;                     ///< What the compiler is given to compile with the toolchain.
    std::vector<std::string> linkArguments;                        ///< What the compiler is given to link with the toolchain.
    std::optional<std::string> ingotIntegrity = std::nullopt;      ///< The digest or SHA-256 the index gave for what was installed.
};

/**
 * @brief Read the text of an install record.
 *
 * Keys this version does not know are passed over. The compiler, and each
 * of the runtime library directories, must be a relative path that stays
 * inside the toolchain's directory. Compile and link arguments are lists
 * of strings. A record without runtime library directories, or without
 * compile or link arguments, names none, and one without an integrity
 * value cannot be compared with the index.
 *
 * @return The record, or where and why it cannot be read.
 */
[[nodiscard]] std::expected<InstallRecord, std::string> parseInstallRecord(std::string_view text);

/**
 * @brief The text of an install record that holds @p record.
 */
[[nodiscard]] std::string renderInstallRecord(const InstallRecord& record);

}  // namespace scrap::Toolchain
