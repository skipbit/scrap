#pragma once

#include <string_view>

namespace scrap::Command {

/// Next step for a path the operating system refused.
inline constexpr std::string_view PermissionHint = "hint: check the permissions of the path\n";

}  // namespace scrap::Command
