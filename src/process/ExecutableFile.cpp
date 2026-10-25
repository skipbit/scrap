#include "process/ExecutableFile.h"

#include <filesystem>
#include <system_error>
#include <unistd.h>

namespace scrap::Process {

bool isExecutableFile(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::status(path, ec);
    if (ec || (! std::filesystem::is_regular_file(status))) {
        return false;
    }
    return (::access(path.c_str(), X_OK) == 0);
}

}  // namespace scrap::Process
