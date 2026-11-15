#pragma once

#include "fetch/UrlFetch.h"

#include <string>
#include <string_view>
#include <system_error>

namespace scrap::Command {

/**
 * @brief The hint for a failure that a second run may well not repeat.
 */
inline constexpr std::string_view RetryHint = "hint: run the command again; if it keeps failing, report it\n";

/**
 * @brief The hint for a failure the operating system reported as @p code.
 */
[[nodiscard]] std::string hintForWriteFailure(const std::error_code& code);

/**
 * @brief The hint for a transfer that failed as @p error.
 */
[[nodiscard]] std::string hintForFetchError(const Fetch::FetchError& error);

}  // namespace scrap::Command
