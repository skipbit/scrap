#pragma once

#include "tool/ExternalTool.h"

#include <expected>
#include <filesystem>
#include <string_view>
#include <vector>

namespace scrap::Command {

/**
 * @brief The tool named @p name on @p searchPaths, with its version, named
 *        on standard error as the tool in use, reporting a tool it did not
 *        find or a version it cannot learn.
 *
 * @return The tool, or the exit code of the failure it reported (1).
 */
[[nodiscard]] std::expected<Tool::ExternalTool, int> lookUpTool(std::string_view name, const std::vector<std::filesystem::path>& searchPaths);

}  // namespace scrap::Command
