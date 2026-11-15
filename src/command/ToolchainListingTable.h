#pragma once

#include "command/ToolchainListing.h"

#include <string>

namespace scrap::Command {

/**
 * @brief @p listing as a table with a heading line, each column as wide as
 *        its widest cell, and no space at the end of a line.
 *
 * The USED HERE column is there only in a project.
 */
[[nodiscard]] std::string renderToolchainListingTable(const ToolchainListing& listing);

}  // namespace scrap::Command
