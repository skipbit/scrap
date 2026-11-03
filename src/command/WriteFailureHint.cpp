#include "command/WriteFailureHint.h"

#include "command/PermissionHint.h"

#include <cerrno>
#include <optional>
#include <string_view>
#include <system_error>

namespace scrap::Command {

namespace {

constexpr std::string_view DiskSpaceHint = "hint: free some disk space and run the command again\n";

/**
 * Whether @p code is the quota failure, which std::errc does not name.
 */
bool isQuotaExceeded(const std::error_code& code)
{
#ifdef EDQUOT
    return ((code.value() == EDQUOT) && ((code.category() == std::generic_category()) || (code.category() == std::system_category())));
#else
    static_cast<void>(code);
    return false;
#endif
}

}  // anonymous namespace

std::optional<std::string_view> sharedWriteHint(const std::error_code& code)
{
    if (code == std::errc::permission_denied || code == std::errc::operation_not_permitted) {
        return PermissionHint;
    }
    if (code == std::errc::no_space_on_device || isQuotaExceeded(code)) {
        return DiskSpaceHint;
    }
    return std::nullopt;
}

}  // namespace scrap::Command
