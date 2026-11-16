#pragma once

#include "install/IndexCopy.h"

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
 * @brief Report why there is no index to choose from.
 */
[[nodiscard]] std::string renderIndexFault(const Install::IndexFault& fault);

/**
 * @brief Report an index that cannot be fetched as @p fault says, the copy
 *        fetched at @p fetchedAt, which is kept as it was, and what to do
 *        about the fault.
 */
[[nodiscard]] std::string renderCopyKept(const Install::IndexUnavailable& fault, std::string_view fetchedAt);

/**
 * @brief Report an index that was fetched but could not be saved.
 */
[[nodiscard]] std::string renderIndexNotSaved(const Install::IndexNotSaved& failure);

}  // namespace scrap::Command
