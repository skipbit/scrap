#pragma once

#include <string>

namespace scrap::Toolchain {

/**
 * @brief Whether @p name is the file name of libc++ as a shared library:
 *        libc++.so and its versions on linux, libc++.*.dylib on darwin.
 */
[[nodiscard]] bool isLibcxxName(const std::string& name);

}  // namespace scrap::Toolchain
