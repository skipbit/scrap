#pragma once

#include "command/RuntimeEnvironment.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace scrap::Command {

/**
 * @brief The directory scrap keeps toolchains and the global default in.
 *
 * @return SCRAP_HOME, or the error and hint lines to report when there is
 *         none.
 */
[[nodiscard]] std::expected<std::filesystem::path, std::string> requireScrapHome(const RuntimeEnvironment& env);

/**
 * @brief Where the global default toolchain is kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path globalConfigFile(const std::filesystem::path& scrapHome);

/**
 * @brief The global default toolchain @p file holds, as written.
 *
 * A file that is not there holds none.
 *
 * @return The default, nothing when none is set, or the error and hint
 *         lines to report when the file cannot be read.
 */
[[nodiscard]] std::expected<std::optional<std::string>, std::string> readGlobalDefault(const std::filesystem::path& file);

/**
 * @brief Set the global default toolchain in @p file to @p toolchain, or
 *        remove it when it is nothing, keeping every other key.
 *
 * The directory is created when it is not there. A file that cannot be read
 * is left as it is.
 *
 * @return Nothing on success, or the error and hint lines to report.
 */
[[nodiscard]] std::expected<void, std::string> writeGlobalDefault(const std::filesystem::path& file, const std::optional<std::string>& toolchain);

}  // namespace scrap::Command
