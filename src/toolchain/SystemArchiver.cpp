#include "toolchain/SystemArchiver.h"

#include "process/Subprocess.h"
#include "toolchain/ProgramAnswer.h"
#include "toolchain/ProgramSearch.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

namespace {

/// The program the compiler is asked to name with -print-prog-name.
constexpr std::string_view ArchiverProgramName = "ar";

/**
 * What @p compiler names as its archiver, without the line end it prints, or
 * nothing when it cannot be asked.
 */
std::optional<std::string> askForArchiver(const std::filesystem::path& compiler)
{
    const std::vector<std::string> arguments{ compiler.string(), "-print-prog-name=" + std::string{ ArchiverProgramName } };
    return askProgram(arguments, { .workingDirectory = {}, .capture = Process::OutputCapture::StandardOutput, .group = Process::ProcessGroup::Caller, .timeout = std::nullopt, .outputLimit = std::nullopt });
}

}  // anonymous namespace

std::expected<std::filesystem::path, NoArchiver> findArchiver(const std::filesystem::path& compiler,
                                                              const std::vector<std::filesystem::path>& systemSearchPaths)
{
    const auto named = askForArchiver(compiler);
    if (! named.has_value()) {
        return std::unexpected(NoArchiver{ .named = std::nullopt });
    }
    const auto found = findProgram(*named, systemSearchPaths);
    if (! found.has_value()) {
        return std::unexpected(NoArchiver{ .named = named });
    }
    return absoluteProgramPath(*found);
}

}  // namespace scrap::Toolchain
