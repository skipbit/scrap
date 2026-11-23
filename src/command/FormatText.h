#pragma once

#include "format/ClangFormat.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Describe why formatting @p file failed.
 *
 * @param tool The clang-format run.
 * @param file The file, relative to the project root.
 * @param failure Why it failed.
 * @return Text for standard error: an "error: ..." line and what clang-format
 *         wrote, each line ending in a newline.
 */
[[nodiscard]] std::string renderFormatFailure(const Tool::ExternalTool& tool, const std::filesystem::path& file, const Tool::ToolFailure& failure);

/**
 * @brief Describe why checking @p file failed.
 *
 * A clang-format that failed is described as for renderFormatFailure(),
 * under a heading that names a check.
 *
 * @param tool The clang-format run.
 * @param file The file, relative to the project root.
 * @param failure Why it failed.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderCheckFailure(const Tool::ExternalTool& tool, const std::filesystem::path& file, const Format::CheckFailure& failure);

/**
 * @brief Say that formatting @p file would change it.
 *
 * @param file The file, relative to the project root.
 * @return A "warning: ..." line ending in a newline.
 */
[[nodiscard]] std::string renderNotFormatted(const std::filesystem::path& file);

/**
 * @brief Say how to format the files a check found unformatted.
 *
 * @return A "hint: ..." line ending in a newline.
 */
[[nodiscard]] std::string renderFormatHint();

}  // namespace scrap::Command
