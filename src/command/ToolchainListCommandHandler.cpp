#include "command/ToolchainListCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainListingTable.h"
#include "command/ToolchainNaming.h"
#include "project/ProjectLoader.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * The error lines @p rendered as a warning, for what a listing goes on
 * without.
 */
std::string asWarning(std::string rendered)
{
    constexpr std::string_view Error = "error: ";
    if (const auto at = rendered.find(Error); at != std::string::npos) {
        rendered.replace(at, Error.size(), "warning: ");
    }
    return rendered;
}

/**
 * The toolchains installed for the host, none when scrap runs on a host
 * no toolchain is built for.
 */
std::expected<std::vector<Toolchain::InstalledName>, Toolchain::CannotListToolchains> installedForHost(const std::filesystem::path& scrapHome)
{
    const auto host = Toolchain::hostEnvironment();
    if (! host.has_value()) {
        return std::vector<Toolchain::InstalledName>{};
    }
    return Toolchain::listInstalledToolchains(scrapHome, *host);
}

/**
 * What names the toolchain used in the project here, nothing outside a
 * project, warning about what cannot be read. A global default that cannot
 * be read is warned about by the caller.
 */
std::optional<NamedHere> readNamedHere(const RuntimeEnvironment& env, const std::expected<std::optional<NamedToolchain>, std::string>& globalDefault)
{
    const auto project = Project::loadProject(env.workingDirectory);
    if (! project.has_value()) {
        if (std::holds_alternative<Project::ProjectNotFound>(project.error())) {
            return std::nullopt;
        }
        std::cerr << asWarning(renderProjectError(project.error()));
        return NameHereUnreadable{};
    }
    auto above = namedAboveGlobalDefault(env, project->manifest.toolchain);
    if (! above.has_value()) {
        std::cerr << asWarning(std::move(above.error()));
        return NameHereUnreadable{};
    }
    if (const std::optional<NamedToolchain>& named = *above; named.has_value()) {
        return *named;
    }
    if (! globalDefault.has_value()) {
        return NameHereUnreadable{};
    }
    if (const std::optional<NamedToolchain>& named = *globalDefault; named.has_value()) {
        return *named;
    }
    return NothingNamedHere{};
}

}  // anonymous namespace

int ToolchainListCommandHandler::execute(const InvocationContext& ctx)
{
    const RuntimeEnvironment& env = *ctx.env;
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    auto installed = installedForHost(*scrapHome);
    if (! installed.has_value()) {
        std::cerr << renderCannotListToolchains(installed.error());
        return 1;
    }
    auto systemCompiler = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    const auto globalDefault = namedGlobalDefault(env);
    if (! globalDefault.has_value()) {
        std::cerr << asWarning(globalDefault.error());
    }
    auto here = readNamedHere(env, globalDefault);

    const ToolchainListing listing = listToolchains({ .installed = std::move(*installed),
                                                      .systemCompiler = systemCompiler.has_value() ? std::optional{ std::move(*systemCompiler) } : std::nullopt,
                                                      .globalDefault = globalDefault.value_or(std::nullopt),
                                                      .here = std::move(here) });
    if (listing.toolchains.empty()) {
        std::cerr << "no toolchain is installed\n"
                     "hint: run 'scrap toolchain install <toolchain>', such as 'gcc'\n";
        return 0;
    }
    std::cout << renderToolchainListingTable(listing);
    return 0;
}

}  // namespace scrap::Command
