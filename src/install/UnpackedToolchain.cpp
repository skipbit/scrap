#include "install/UnpackedToolchain.h"

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
    return name.starts_with("libstdc++.so") || name.starts_with("libc++.so") || (name.starts_with("libc++.") && name.ends_with(".dylib"));
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
    for (const std::string_view top : { "lib", "lib64" }) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(root / top, ec), end; (! ec) && (it != end); it.increment(ec)) {
            if (isRuntimeLibraryName(it->path().filename().string())) {
                found.push_back(it->path().parent_path().lexically_relative(root));
            }
        }
    }
    std::ranges::sort(found);
    const auto repeated = std::ranges::unique(found);
    found.erase(repeated.begin(), repeated.end());
    return found;
}

}  // namespace scrap::Install
