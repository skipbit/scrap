#include "check/ClangTidy.h"

#include "tool/ExternalTool.h"
#include "toolchain/CommandArgument.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace scrap::Check {

namespace {

/**
 * The command line that runs @p tool on @p file.
 *
 * --checks adds to the checks of .clang-tidy, so -* comes first to put
 * @p checks in their place.
 */
std::vector<std::string> commandFor(const Tool::ExternalTool& tool,
                                    const std::filesystem::path& databaseDirectory,
                                    const std::filesystem::path& file,
                                    const std::optional<std::string>& checks)
{
    std::vector<std::string> arguments{ tool.program.string(), "--quiet", "-p=" + databaseDirectory.string() };
    if (checks.has_value()) {
        arguments.push_back("--checks=-*," + *checks);
    }
    arguments.push_back(Toolchain::pathAsArgument(file).string());
    return arguments;
}

}  // anonymous namespace

std::expected<std::string, Tool::ToolFailure> analyzeFile(const Tool::ExternalTool& tool,
                                                          const std::filesystem::path& projectRoot,
                                                          const std::filesystem::path& databaseDirectory,
                                                          const std::filesystem::path& file,
                                                          const std::optional<std::string>& checks)
{
    return Tool::runTool(commandFor(tool, databaseDirectory, file, checks), projectRoot);
}

}  // namespace scrap::Check
