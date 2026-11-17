#include "install/ToolchainRemoval.h"

#include "install/WorkDirectory.h"
#include "toolchain/ScrapHomeLayout.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <system_error>

namespace scrap::Install {

namespace {

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
    const auto work = WorkDirectory::below(workRoot, "uninstall");
    if (! work.has_value()) {
        return std::unexpected(ToolchainNotRemoved{ .path = workRoot, .code = work.error().code });
    }
    std::error_code ec;
    std::filesystem::rename(directory, work->path() / "removed", ec);
    if (ec) {
        return std::unexpected(ToolchainNotRemoved{ .path = directory, .code = ec });
    }
    removeEmptyUpTo(directory.parent_path(), Toolchain::toolchainsDirectory(scrapHome));
    return {};
}

}  // namespace scrap::Install
