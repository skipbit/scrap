#include "command/ToolLookup.h"

#include "command/ToolText.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Command {

std::expected<Tool::ExternalTool, int> lookUpTool(const std::string_view name, const std::vector<std::filesystem::path>& searchPaths)
{
    const auto program = Tool::findTool(name, searchPaths);
    if (! program.has_value()) {
        std::cerr << renderToolNotFound(name);
        return std::unexpected(1);
    }
    auto tool = Tool::describeTool(name, *program);
    if (! tool.has_value()) {
        std::cerr << renderVersionFailure(name, *program, tool.error());
        return std::unexpected(1);
    }
    std::cerr << renderToolInUse(*tool);
    return std::move(*tool);
}

}  // namespace scrap::Command
