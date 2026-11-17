#include "command/ToolchainUninstallCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/NameAfterUninstall.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainNaming.h"
#include "command/ToolchainUninstallText.h"
#include "install/ToolchainRemoval.h"
#include "project/ProjectLoader.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainSpecifier.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * What SCRAP_TOOLCHAIN or the project here names for builds, in place of
 * the global default. Nothing when neither does, or when what names it
 * cannot be read, since then a build here stops whatever is installed.
 */
std::optional<NamedToolchain> namedAboveHere(const RuntimeEnvironment& env)
{
    std::optional<std::string> projectToolchain;
    if (const auto project = Project::loadProject(env.workingDirectory); project.has_value()) {
        projectToolchain = project->manifest.toolchain;
    } else if (! std::holds_alternative<Project::ProjectNotFound>(project.error())) {
        return std::nullopt;
    }
    auto above = namedAboveGlobalDefault(env, projectToolchain);
    if (! above.has_value()) {
        return std::nullopt;
    }
    return std::move(*above);
}

/**
 * Unselect the global default when @p after says it leads nowhere now, and
 * say what became of it.
 *
 * @return Whether it is told without a failure.
 */
bool settleGlobalDefault(const std::filesystem::path& scrapHome, const NamedToolchain& named, const NameAfterUninstall& after)
{
    if (std::holds_alternative<LeadsNowhere>(after)) {
        const std::filesystem::path file = Toolchain::globalConfigFile(scrapHome);
        if (auto written = Toolchain::writeGlobalDefaultFile(file, std::nullopt); ! written.has_value()) {
            std::cerr << renderGlobalConfigFileError(file, written.error());
            return false;
        }
    }
    std::cerr << renderGlobalDefaultAfterUninstall(named, after);
    return true;
}

/**
 * The one toolchain to uninstall, and what is installed for the host
 * beside it.
 */
struct ToUninstall {
    std::vector<Toolchain::InstalledName> installed;
    Toolchain::InstalledPlace place;
};

/**
 * The one installed toolchain @p specifier, typed as @p text, names, or the
 * error lines when it names none or more than one.
 */
std::expected<ToUninstall, std::string> oneToUninstall(const std::filesystem::path& scrapHome,
                                                       const Toolchain::HostEnvironment& host,
                                                       const Toolchain::ToolchainSpecifier& specifier,
                                                       const std::string& text)
{
    auto installed = Toolchain::listInstalledToolchains(scrapHome, host);
    if (! installed.has_value()) {
        return std::unexpected(renderCannotListToolchains(installed.error()));
    }
    auto named = Toolchain::installedNamedBy(scrapHome, host, specifier);
    if (! named.has_value()) {
        return std::unexpected(renderCannotListToolchains(named.error()));
    }
    if (named->empty()) {
        return std::unexpected(renderNothingToUninstall(text));
    }
    if (named->size() > 1) {
        std::vector<std::string> names;
        for (const Toolchain::InstalledPlace& place : *named) {
            names.push_back(place.name.text());
        }
        return std::unexpected(renderNamesMoreThanOneToUninstall(text, names));
    }
    return ToUninstall{ .installed = std::move(*installed), .place = std::move(named->front()) };
}

/**
 * Say what became of the global default @p byDefault and of what names the
 * toolchain for builds here, when either led to @p removed among
 * @p installed, unselecting the default when it leads nowhere now.
 *
 * @return Whether it is told without a failure.
 */
bool tellWhereNamesLead(const RuntimeEnvironment& env,
                        const std::filesystem::path& scrapHome,
                        const std::optional<NamedToolchain>& byDefault,
                        const std::vector<Toolchain::InstalledName>& installed,
                        const std::string& removed)
{
    bool told = true;
    if (byDefault.has_value()) {
        if (const auto after = nameAfterUninstall(*byDefault, installed, removed); after.has_value()) {
            told = settleGlobalDefault(scrapHome, *byDefault, *after);
        }
    }
    if (const auto above = namedAboveHere(env); above.has_value()) {
        if (const auto after = nameAfterUninstall(*above, installed, removed); after.has_value()) {
            std::cerr << renderUsedHereAfterUninstall(*above, *after);
        }
    }
    return told;
}

}  // anonymous namespace

int ToolchainUninstallCommandHandler::execute(const InvocationContext& ctx)
{
    const RuntimeEnvironment& env = *ctx.env;
    // The parser requires the specifier, so the first positional is there.
    const std::string text = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();
    const auto choice = Toolchain::parseToolchainChoice(text);
    if (! choice.has_value()) {
        std::cerr << renderNotASpecifier(text, choice.error());
        return 1;
    }
    const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&*choice);
    if (specifier == nullptr) {
        std::cerr << renderCannotUninstallSystem();
        return 1;
    }
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    const auto host = Toolchain::hostEnvironment();
    if (! host.has_value()) {
        std::cerr << renderNoToolchainForHost(ToolchainNamedIn::CommandLine);
        return 1;
    }
    const auto globalDefault = namedGlobalDefault(env);
    if (! globalDefault.has_value()) {
        std::cerr << globalDefault.error();
        return 1;
    }
    const auto chosen = oneToUninstall(*scrapHome, *host, *specifier, text);
    if (! chosen.has_value()) {
        std::cerr << chosen.error();
        return 1;
    }
    const std::string removed = chosen->place.name.text();
    if (auto taken = Install::removeInstalledToolchain(*scrapHome, chosen->place.directory); ! taken.has_value()) {
        std::cerr << renderNotRemoved(taken.error());
        return 1;
    }
    std::cerr << renderUninstalled(removed, chosen->place.directory);
    return tellWhereNamesLead(env, *scrapHome, *globalDefault, chosen->installed, removed) ? 0 : 1;
}

}  // namespace scrap::Command
