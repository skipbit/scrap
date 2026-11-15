#pragma once

#include "toolchain/IndexChanges.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Report the index fetched at @p fetchedAt, and what changed for the
 *        host named @p hostName since the copy fetched before it.
 */
[[nodiscard]] std::string renderIndexChanges(std::string_view fetchedAt, std::string_view hostName, const Toolchain::IndexChanges& changes);

/**
 * @brief Report the index fetched at @p fetchedAt with no copy before it to
 *        compare with, and how many toolchains the host named @p hostName
 *        can install from it.
 */
[[nodiscard]] std::string renderFirstIndex(std::string_view fetchedAt, std::string_view hostName, std::size_t installable);

}  // namespace scrap::Command
