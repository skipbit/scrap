#include "install/ToolchainRemoval.h"

#include "toolchain/ScrapHomeLayout.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

namespace scrap::Install {

namespace {

/**
 * A name below @p workRoot that nothing holds, for this process.
 */
std::filesystem::path unusedName(const std::filesystem::path& workRoot)
{
    const std::string prefix = "uninstall-" + std::to_string(::getpid()) + "-";
    for (unsigned count = 0;; ++count) {
        std::filesystem::path candidate = workRoot / (prefix + std::to_string(count));
        std::error_code ec;
        if (! std::filesystem::exists(std::filesystem::symlink_status(candidate, ec))) {
            return candidate;
        }
    }
}

/**
 * Remove @p directory and the parents above it up to @p top while each is
 * empty.
 */
void removeEmptyUpTo(std::filesystem::path directory, const std::filesystem::path& top)
{
    while (directory != top && directory.has_relative_path()) {
        std::error_code ec;
        if (! std::filesystem::is_empty(directory, ec) || ec) {
            return;
        }
        std::filesystem::remove(directory, ec);
        if (ec) {
            return;
        }
        directory = directory.parent_path();
    }
}

}  // anonymous namespace

std::expected<void, ToolchainNotRemoved> removeInstalledToolchain(const std::filesystem::path& scrapHome, const std::filesystem::path& directory)
{
    const std::filesystem::path workRoot = Toolchain::temporaryDirectory(scrapHome);
    std::error_code ec;
    std::filesystem::create_directories(workRoot, ec);
    if (ec) {
        return std::unexpected(ToolchainNotRemoved{ .path = workRoot, .code = ec });
    }
    const std::filesystem::path setAside = unusedName(workRoot);
    std::filesystem::rename(directory, setAside, ec);
    if (ec) {
        return std::unexpected(ToolchainNotRemoved{ .path = directory, .code = ec });
    }
    std::filesystem::remove_all(setAside, ec);
    removeEmptyUpTo(directory.parent_path(), Toolchain::toolchainsDirectory(scrapHome));
    return {};
}

}  // namespace scrap::Install
