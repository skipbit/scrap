#pragma once

#include <filesystem>

namespace scrap::Toolchain {

/**
 * @brief @p path as it goes on the command line.
 *
 * A relative path that starts with '-' or '@' gains a leading ./. An argument
 * starting with '@' names a file of options read before anything else, so a
 * file name could decide the command, and one starting with '-' is read as an
 * option. Compilers and the LLVM tools read their command lines this way.
 */
[[nodiscard]] std::filesystem::path pathAsArgument(const std::filesystem::path& path);

}  // namespace scrap::Toolchain
