#pragma once

#include "toolchain/GlobalConfig.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <variant>

namespace scrap::Toolchain {

/**
 * @brief config.toml exists and cannot be read.
 */
struct CannotReadConfig {
    std::filesystem::path path;
};

/**
 * @brief The directory config.toml belongs in cannot be created.
 */
struct CannotCreateDirectory {
    std::filesystem::path path;
    std::error_code code;
};

/**
 * @brief config.toml cannot be written.
 */
struct CannotWriteConfig {
    std::filesystem::path path;
    std::error_code code;
};

/**
 * @brief Why config.toml cannot be read or written.
 */
using GlobalConfigFileError = std::variant<GlobalConfigFault, CannotReadConfig, CannotCreateDirectory, CannotWriteConfig>;

/**
 * @brief The global default toolchain @p file holds, as written.
 *
 * A file that is not there holds none.
 *
 * @return The default, nothing when none is set, or why the file cannot be
 *         read.
 */
[[nodiscard]] std::expected<std::optional<std::string>, GlobalConfigFileError> readGlobalDefaultFile(const std::filesystem::path& file);

/**
 * @brief Set the global default toolchain in @p file to @p toolchain, or
 *        remove it when it is nothing, keeping every other key.
 *
 * The directory is created when it is not there. A file that cannot be read
 * is left as it is.
 *
 * @return Nothing on success, or why the file cannot be read or written.
 */
[[nodiscard]] std::expected<void, GlobalConfigFileError> writeGlobalDefaultFile(const std::filesystem::path& file, const std::optional<std::string>& toolchain);

}  // namespace scrap::Toolchain
