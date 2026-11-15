#pragma once

#include "toolchain/IndexChanges.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief The words that tell when an index was fetched: @p fetchedAt, or
 *        that the time is unknown.
 */
[[nodiscard]] std::string fetchTimeText(const std::optional<std::string>& fetchedAt);

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

/**
 * @brief Report an index that cannot be fetched for @p reason, and the copy
 *        fetched at @p fetchedAt, which is kept as it was.
 */
[[nodiscard]] std::string renderCopyKept(std::string_view reason, std::string_view fetchedAt);

}  // namespace scrap::Command
