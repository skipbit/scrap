#pragma once

#include <string>
#include <vector>

namespace scrap::Command {

/**
 * @brief @p rows laid out in columns, each as wide as its widest cell and
 *        apart from the next by two spaces, with no spaces ending a line.
 *
 * @param rows At least one row, each with as many cells as the first.
 */
[[nodiscard]] std::string layOutColumns(const std::vector<std::vector<std::string>>& rows);

}  // namespace scrap::Command
