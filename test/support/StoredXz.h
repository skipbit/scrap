#pragma once

#include <string>
#include <string_view>

namespace scrap::TestSupport {

/**
 * @brief @p content as an xz file holding it uncompressed, for a test that needs
 *        an archive made on the spot.
 */
[[nodiscard]] std::string storedXz(std::string_view content);

}  // namespace scrap::TestSupport
