#pragma once

#include <span>
#include <string>

namespace scrap::Command {

/**
 * @brief A command path as the user types it.
 *
 * @param names Names of the command and the commands above it, outermost
 *        first.
 * @return The names separated by spaces.
 */
[[nodiscard]] std::string typedPath(std::span<const std::string> names);

}  // namespace scrap::Command
