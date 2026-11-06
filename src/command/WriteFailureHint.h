#pragma once

#include <optional>
#include <string_view>
#include <system_error>

namespace scrap::Command {

/**
 * @brief The next step every write failure shares: the permissions, or the
 *        space left. Any other reason is left to the caller.
 */
[[nodiscard]] std::optional<std::string_view> sharedWriteHint(const std::error_code& code);

}  // namespace scrap::Command
