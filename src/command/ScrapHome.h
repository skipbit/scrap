#pragma once

#include "command/RuntimeEnvironment.h"

#include <expected>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief The directory scrap keeps toolchains and the global default in.
 *
 * @return SCRAP_HOME, or the error and hint lines to report when there is
 *         none.
 */
[[nodiscard]] std::expected<std::filesystem::path, std::string> requireScrapHome(const RuntimeEnvironment& env);

}  // namespace scrap::Command
