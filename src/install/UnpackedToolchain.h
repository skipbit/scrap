#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Install {

/**
 * @brief Where the toolchain starts in what was unpacked into @p directory: an
 *        archive may hold it all in one directory of its own.
 */
[[nodiscard]] std::filesystem::path contentRoot(const std::filesystem::path& directory);

/**
 * @brief The directories below @p root, relative to it, that hold a C++ runtime
 *        library.
 */
[[nodiscard]] std::vector<std::filesystem::path> findRuntimeLibraryDirectories(const std::filesystem::path& root);

/**
 * @brief What a compiler is given to build with the runtime libraries of its
 *        toolchain.
 */
struct RuntimeArguments {
    std::vector<std::string> compile;  ///< Given to every compile.
    std::vector<std::string> link;     ///< Given to every link.
};

/**
 * @brief What the compiler of the toolchain at @p root needs to build with
 *        the runtime libraries the toolchain holds, on @p platform.
 *
 * A toolchain that holds libc++, as LLVM does, is asked to use it. On linux
 * the compiler is also asked for the linker, compiler-rt and libunwind of
 * the toolchain, so nothing comes from a GCC on the host. On darwin it is
 * asked for the libc++abi of the toolchain, which its libc++ does not
 * re-export, and keeps the linker of the system, which reads the SDK. A
 * toolchain that holds no libc++, as GCC, needs nothing.
 *
 * @param platform The platform of the ingot, as the index spells it.
 * @param root Where the toolchain starts.
 */
[[nodiscard]] RuntimeArguments runtimeArgumentsFor(std::string_view platform, const std::filesystem::path& root);

}  // namespace scrap::Install
