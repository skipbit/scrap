#include "TempDirectory.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>

namespace scrap::TestSupport {

/**
 * Create the directory with mkdtemp(3), which creates a uniquely named
 * directory in place of the trailing "XXXXXX". Picking a name and then
 * creating it leaves a window in which another process can take the same
 * name, and under ctest --parallel each test is its own process, so that
 * window is real. This mirrors the scheme the end-to-end fixture uses.
 */
TempDirectory::TempDirectory()
{
    std::error_code ec;
    const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
    if (ec) {
        ADD_FAILURE() << "no usable temp directory: " << ec.message();
        return;
    }

    std::string dirTemplate = (base / "scrap_project_test_XXXXXX").string();
    const char* created = ::mkdtemp(dirTemplate.data());
    if (created == nullptr) {
        ADD_FAILURE() << "mkdtemp failed: " << std::strerror(errno);
        return;
    }
    path_ = std::filesystem::path(created);
}

TempDirectory::~TempDirectory()
{
    if (path_.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    if (ec) {
        ADD_FAILURE() << "cannot remove " << path_ << ": " << ec.message();
    }
}

const std::filesystem::path& TempDirectory::path() const
{
    return path_;
}

std::filesystem::path TempDirectory::writeFile(std::string_view relative, std::string_view content) const
{
    const std::filesystem::path target = path_ / relative;

    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
        ADD_FAILURE() << "cannot create " << target.parent_path() << ": " << ec.message();
        return target;
    }

    std::ofstream output(target, std::ios::binary);
    if (! output.is_open()) {
        ADD_FAILURE() << "cannot open " << target << " for writing";
        return target;
    }
    output << content;
    output.close();
    if (! output) {
        ADD_FAILURE() << "cannot write " << target;
    }
    return target;
}

std::filesystem::path TempDirectory::makeDirectory(std::string_view relative) const
{
    const std::filesystem::path target = path_ / relative;
    std::error_code ec;
    std::filesystem::create_directories(target, ec);
    if (ec) {
        ADD_FAILURE() << "cannot create " << target << ": " << ec.message();
    }
    return target;
}

}  // namespace scrap::TestSupport
