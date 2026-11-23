#pragma once

#include "tool/ExternalTool.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief The line naming the tool a command runs, and its version.
 *
 * @param tool The tool, named as the messages name it.
 * @return "Using <name> '<program>' (version <version>)", ending in a
 *         newline.
 */
[[nodiscard]] std::string renderToolInUse(const Tool::ExternalTool& tool);

/**
 * @brief Say that no tool named @p name was found, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderToolNotFound(std::string_view name);

/**
 * @brief Describe why @p program did not give its version.
 *
 * @param name The tool as the messages name it.
 * @param program The program asked.
 * @param failure Why it did not answer.
 * @return Text for standard error: an "error: ..." line and what the program
 *         wrote, each line ending in a newline.
 */
[[nodiscard]] std::string renderVersionFailure(std::string_view name, const std::filesystem::path& program, const Tool::ToolFailure& failure);

/**
 * @brief The line under @p heading for @p tool, which failed.
 *
 * It names the signal that stopped the tool, or its exit status when it
 * wrote nothing. What the tool wrote is not included.
 *
 * @param tool The tool run.
 * @param heading An "error: ..." line, without its newline.
 * @param failed How it failed.
 * @return The line, ending in a newline.
 */
[[nodiscard]] std::string renderToolFailureHeading(const Tool::ExternalTool& tool, const std::string& heading, const Tool::ToolFailed& failed);

/**
 * @brief Describe why running @p tool failed, under @p heading.
 *
 * A program that could not be started is named with the system's reason,
 * and @p heading is left out. One that failed comes under @p heading, with
 * the signal that stopped it, or its exit status when it wrote nothing, and
 * then what it wrote.
 *
 * @param tool The tool run.
 * @param heading An "error: ..." line, without its newline.
 * @param failure Why it failed.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderToolFailure(const Tool::ExternalTool& tool, const std::string& heading, const Tool::ToolFailure& failure);

}  // namespace scrap::Command
