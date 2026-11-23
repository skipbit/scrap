#pragma once

#include "tool/ExternalTool.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Check {

/// The name clang-tidy is looked for by, and the messages name it by.
inline constexpr std::string_view ClangTidyName = "clang-tidy";

/**
 * @brief Analyze @p file with clang-tidy, by every command that compiles it.
 *
 * clang-tidy runs in @p projectRoot, reads the commands from the
 * compile_commands.json in @p databaseDirectory, and leaves out the count of
 * warnings it would otherwise write where it can.
 *
 * @param tool The clang-tidy to run.
 * @param projectRoot Directory clang-tidy runs in.
 * @param databaseDirectory Directory holding compile_commands.json, relative
 *        to @p projectRoot.
 * @param file File to analyze, relative to @p projectRoot.
 * @param checks The checks to enable in place of those of .clang-tidy, in
 *        clang-tidy's notation; nothing leaves .clang-tidy to choose them.
 *        The other settings of .clang-tidy stay in effect either way.
 * @return What clang-tidy wrote, both streams together, when it exited with
 *         0. Otherwise the failure, which holds what it wrote; an exit
 *         status other than 0 also comes from a diagnostic clang-tidy judged
 *         an error.
 */
[[nodiscard]] std::expected<std::string, Tool::ToolFailure> analyzeFile(const Tool::ExternalTool& tool,
                                                                        const std::filesystem::path& projectRoot,
                                                                        const std::filesystem::path& databaseDirectory,
                                                                        const std::filesystem::path& file,
                                                                        const std::optional<std::string>& checks);

}  // namespace scrap::Check
