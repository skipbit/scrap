#include "command/FormatText.h"

#include "command/PrintableText.h"
#include "command/ToolText.h"
#include "format/ClangFormat.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <string>
#include <type_traits>
#include <variant>

namespace scrap::Command {

namespace {

/// The heading of a file clang-format failed on.
std::string failedToFormat(const std::filesystem::path& file)
{
    return "error: failed to format '" + printablePath(file) + "'";
}

/// The heading of a file clang-format failed on during a check.
std::string failedToCheck(const std::filesystem::path& file)
{
    return "error: failed to check '" + printablePath(file) + "'";
}

}  // anonymous namespace

std::string renderFormatFailure(const Tool::ExternalTool& tool, const std::filesystem::path& file, const Tool::ToolFailure& failure)
{
    return renderToolFailure(tool, failedToFormat(file), failure);
}

std::string renderCheckFailure(const Tool::ExternalTool& tool, const std::filesystem::path& file, const Format::CheckFailure& failure)
{
    return std::visit([&tool, &file](const auto& alternative) -> std::string {
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(alternative)>, Format::FileUnreadable>) {
            return "error: cannot read '" + printablePath(file) + "' to compare it with what clang-format wrote\n";
        } else {
            return renderToolFailure(tool, failedToCheck(file), Tool::ToolFailure{ alternative });
        }
    }, failure);
}

std::string renderNotFormatted(const std::filesystem::path& file)
{
    return "warning: '" + printablePath(file) + "' is not formatted\n";
}

std::string renderFormatHint()
{
    return "hint: run the same command without --check to format the files\n";
}

}  // namespace scrap::Command
