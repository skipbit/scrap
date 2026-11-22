#pragma once

#include "format/ClangFormat.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief The line naming the clang-format a format runs, and its version.
 *
 * @return "Using clang-format '<program>' (version <version>)", ending in a
 *         newline.
 */
[[nodiscard]] std::string renderClangFormatInUse(const Format::ClangFormat& tool);

/**
 * @brief Say that no clang-format was found, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderClangFormatNotFound();

/**
 * @brief Describe why @p program did not give its version.
 *
 * @param program The clang-format asked.
 * @param failure Why it did not answer.
 * @return Text for standard error: an "error: ..." line and what the program
 *         wrote, each line ending in a newline.
 */
[[nodiscard]] std::string renderVersionFailure(const std::filesystem::path& program, const Format::ToolFailure& failure);

/**
 * @brief Describe why formatting @p file failed.
 *
 * @param tool The clang-format run.
 * @param file The file, relative to the project root.
 * @param failure Why it failed.
 * @return Text for standard error: an "error: ..." line and what clang-format
 *         wrote, each line ending in a newline.
 */
[[nodiscard]] std::string renderFormatFailure(const Format::ClangFormat& tool, const std::filesystem::path& file, const Format::ToolFailure& failure);

}  // namespace scrap::Command
