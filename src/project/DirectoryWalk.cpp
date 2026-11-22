#include "project/DirectoryWalk.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <system_error>

namespace scrap::Project {

std::expected<void, SourceScanFailure> walkDirectory(const std::filesystem::path& projectRoot,
                                                     const std::filesystem::path& directory,
                                                     const WalkVisitor& visit)
{
    const std::filesystem::path walked = projectRoot / directory;
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(walked, ec);
    if (! std::filesystem::status_known(status)) {
        return std::unexpected(SourceScanFailure{ .directory = walked, .reason = ec.message() });
    }
    if (! std::filesystem::is_directory(status)) {
        return {};
    }

    std::filesystem::recursive_directory_iterator it(walked, ec);
    if (ec) {
        return std::unexpected(SourceScanFailure{ .directory = walked, .reason = ec.message() });
    }
    // The failure is read straight after the step that caused it: an increment
    // that fails leaves the iterator at the end, so a check at the top of the
    // loop would never run and the walk would stop as though it had finished.
    for (const std::filesystem::recursive_directory_iterator end; it != end;) {
        const std::filesystem::path current = it->path();
        const std::filesystem::path relative = current.lexically_relative(projectRoot).lexically_normal();
        if (visit(*it, relative) == WalkStep::SkipBelow) {
            it.disable_recursion_pending();
        }
        it.increment(ec);
        if (ec) {
            return std::unexpected(SourceScanFailure{ .directory = current, .reason = ec.message() });
        }
    }
    return {};
}

}  // namespace scrap::Project
