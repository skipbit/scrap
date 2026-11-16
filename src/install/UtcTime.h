#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace scrap::Install {

/**
 * @brief @p time in UTC to the second, as 2026-11-12T11:37:11Z, or nothing
 *        when the system cannot break it into a date.
 */
[[nodiscard]] std::optional<std::string> utcText(std::chrono::system_clock::time_point time);

}  // namespace scrap::Install
