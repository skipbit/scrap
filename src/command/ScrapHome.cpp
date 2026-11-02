#include "command/ScrapHome.h"

#include "command/RuntimeEnvironment.h"
#include "command/ToolchainDiagnostic.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>

namespace scrap::Command {

std::expected<std::filesystem::path, std::string> requireScrapHome(const RuntimeEnvironment& env)
{
    if (! env.scrapHome.has_value()) {
        return std::unexpected(renderNoScrapHome());
    }
    return *env.scrapHome;
}

}  // namespace scrap::Command
