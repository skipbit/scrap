#include "build/BuildOutput.h"

#include "compile/CompilePlanner.h"

#include <filesystem>
#include <string_view>
#include <system_error>

namespace scrap::Build {

// clean removes what build writes only while the one lies inside the other.
static_assert(Compile::DebugBuildDirectory.starts_with(OutputDirectory) && (Compile::DebugBuildDirectory.size() > OutputDirectory.size())
                  && (Compile::DebugBuildDirectory[OutputDirectory.size()] == '/'),
              "the debug build directory must lie inside the build output directory");

std::expected<OutputRemoval, OutputRemovalFailure> removeOutput(const std::filesystem::path& directory)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(directory, ec);
    if (status.type() == std::filesystem::file_type::not_found) {
        return OutputRemoval::NothingThere;
    }
    if (ec) {
        return std::unexpected{ OutputRemovalFailure{ .problem = OutputRemovalProblem::CannotInspect, .path = directory, .code = ec } };
    }

    switch (status.type()) {
    case std::filesystem::file_type::directory:
        std::filesystem::remove_all(directory, ec);
        break;
    case std::filesystem::file_type::symlink:
        std::filesystem::remove(directory, ec);
        break;
    default:
        return std::unexpected{ OutputRemovalFailure{ .problem = OutputRemovalProblem::NotADirectory, .path = directory, .code = {} } };
    }
    if (ec) {
        return std::unexpected{ OutputRemovalFailure{ .problem = OutputRemovalProblem::CannotRemove, .path = directory, .code = ec } };
    }
    return OutputRemoval::Removed;
}

}  // namespace scrap::Build
