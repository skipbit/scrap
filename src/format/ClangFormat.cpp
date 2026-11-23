#include "format/ClangFormat.h"

#include "file/WholeFile.h"
#include "tool/ExternalTool.h"
#include "toolchain/CommandArgument.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Format {

namespace {

/**
 * The command line that runs @p tool on @p file, after @p options.
 */
std::vector<std::string> commandFor(const Tool::ExternalTool& tool,
                                    const std::vector<std::string>& options,
                                    const std::filesystem::path& file,
                                    const std::optional<std::string>& style)
{
    std::vector<std::string> arguments{ tool.program.string() };
    arguments.insert(arguments.end(), options.begin(), options.end());
    if (style.has_value()) {
        arguments.push_back("--style=" + *style);
    }
    arguments.push_back(Toolchain::pathAsArgument(file).string());
    return arguments;
}

}  // anonymous namespace

std::expected<void, Tool::ToolFailure> formatFile(const Tool::ExternalTool& tool,
                                                  const std::filesystem::path& projectRoot,
                                                  const std::filesystem::path& file,
                                                  const std::optional<std::string>& style)
{
    const auto output = Tool::runTool(commandFor(tool, { "-i" }, file, style), projectRoot);
    if (! output.has_value()) {
        return std::unexpected(output.error());
    }
    return {};
}

std::expected<FileState, CheckFailure> checkFile(const Tool::ExternalTool& tool,
                                                 const std::filesystem::path& projectRoot,
                                                 const std::filesystem::path& file,
                                                 const std::optional<std::string>& style)
{
    const auto formatted = Tool::standardOutputOf(commandFor(tool, {}, file, style), projectRoot);
    if (! formatted.has_value()) {
        return std::unexpected(std::visit([](const auto& failure) {
            return CheckFailure{ failure };
        }, formatted.error()));
    }
    const auto content = File::readWholeFile(projectRoot / file);
    if (! content.has_value()) {
        return std::unexpected(FileUnreadable{});
    }
    return (*content == *formatted) ? FileState::Formatted : FileState::Unformatted;
}

}  // namespace scrap::Format
