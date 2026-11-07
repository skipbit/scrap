#include "install/UnpackedToolchain.h"

#include "toolchain/RuntimeLibraryName.h"
#include "toolchain/ToolchainArguments.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Install {

namespace {

bool isRuntimeLibraryName(const std::string& name)
{
    return name.starts_with("libstdc++.so") || Toolchain::isLibcxxName(name);
}

/**
 * The files below lib and lib64 of @p root whose names @p accepts takes.
 */
std::vector<std::filesystem::path> librariesBelow(const std::filesystem::path& root, bool (*accepts)(const std::string&))
{
    std::vector<std::filesystem::path> found;
    for (const std::string_view top : { "lib", "lib64" }) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(root / top, ec), end; (! ec) && (it != end); it.increment(ec)) {
            if (accepts(it->path().filename().string())) {
                found.push_back(it->path());
            }
        }
    }
    return found;
}

}  // anonymous namespace

/**
 * Where the toolchain starts in what was unpacked into @p directory: an archive
 * may hold it all in one directory of its own.
 */
std::filesystem::path contentRoot(const std::filesystem::path& directory)
{
    std::error_code ec;
    std::filesystem::directory_iterator entries(directory, ec);
    if (ec || (entries == std::filesystem::directory_iterator{})) {
        return directory;
    }
    std::filesystem::path only = entries->path();
    entries.increment(ec);
    if (ec || (entries != std::filesystem::directory_iterator{}) || (only.filename() == "bin")
        || (! std::filesystem::is_directory(std::filesystem::symlink_status(only, ec)))) {
        return directory;
    }
    return only;
}

/**
 * The directories below @p root, relative to it, that hold a C++ runtime library.
 */
std::vector<std::filesystem::path> findRuntimeLibraryDirectories(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> found;
    for (const std::filesystem::path& library : librariesBelow(root, isRuntimeLibraryName)) {
        found.push_back(library.parent_path().lexically_relative(root));
    }
    std::ranges::sort(found);
    const auto repeated = std::ranges::unique(found);
    found.erase(repeated.begin(), repeated.end());
    return found;
}

Toolchain::ToolchainArguments runtimeArgumentsFor(std::string_view platform, const std::filesystem::path& root)
{
    if (librariesBelow(root, Toolchain::isLibcxxName).empty()) {
        return Toolchain::ToolchainArguments{ .compile = {}, .link = {} };
    }
    if (platform == "darwin") {
        return Toolchain::ToolchainArguments{ .compile = {}, .link = { "-lc++abi" } };
    }
    return Toolchain::ToolchainArguments{ .compile = { "-stdlib=libc++" },
                                          .link = { "-stdlib=libc++", "-fuse-ld=lld", "-rtlib=compiler-rt", "-unwindlib=libunwind" } };
}

}  // namespace scrap::Install
