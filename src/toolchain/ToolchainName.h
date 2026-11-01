#pragma once

#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief Whether @p text can stand as a family, compiler, platform or
 * architecture name.
 *
 * Such a name becomes one component of a path below the toolchain store, so
 * it holds only letters, digits, '-', '_', '.' and '+', and is not '.' or
 * '..'.
 */
[[nodiscard]] bool isToolchainName(std::string_view text);

}  // namespace scrap::Toolchain
