#pragma once

#include <string_view>

namespace scrap::Command {

/// Next step after a diagnostic, which already says what is wrong.
inline constexpr std::string_view FixErrorsHint = "hint: fix the errors reported above and run the command again\n";

}  // namespace scrap::Command
