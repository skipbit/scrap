#include "command/ToolchainListCommandHandler.h"

#include "command/InstallingHostOfScrap.h"
#include "command/InvocationContext.h"
#include "command/ParsedOptions.h"
#include "command/RemoteListing.h"
#include "command/RemoteListingText.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainIndexDiagnostic.h"
#include "command/ToolchainIndexLocation.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainListingFacts.h"
#include "command/ToolchainListingTable.h"
#include "install/IndexCopy.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstalledToolchain.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <optional>
#include <utility>

namespace scrap::Command {

namespace {

int listInstalled(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome)
{
    auto facts = readListingFacts(env, scrapHome);
    if (! facts.has_value()) {
        std::cerr << facts.error();
        return 1;
    }
    const ToolchainListing listing = listToolchains(*facts);
    if (listing.toolchains.empty()) {
        std::cerr << "no toolchain is installed\n"
                     "hint: run 'scrap toolchain install <toolchain>', such as 'gcc'\n";
        return 0;
    }
    std::cout << renderToolchainListingTable(listing);
    return 0;
}

/**
 * List the toolchains of the saved copy of the index, the ones the host
 * can install or, when @p all, every one.
 */
int listRemote(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome, const bool all)
{
    const auto environment = Toolchain::hostEnvironment();
    if (! environment.has_value()) {
        std::cerr << renderNoToolchainForHost(ToolchainNamedIn::CommandLine);
        return 1;
    }
    const Toolchain::InstallingHost host = installingHostOfScrap(*environment);
    auto saved = Install::readSavedIndex(scrapHome, toolchainIndexUrl(env), host.scrapVersion);
    if (! saved.has_value()) {
        std::cerr << renderIndexFault(Install::IndexFault{ std::move(saved.error()) });
        return 1;
    }
    const std::optional<Install::SavedIndex>& copy = *saved;
    if (! copy.has_value()) {
        std::cerr << renderIndexNotFetched();
        return 1;
    }
    const auto installed = Toolchain::listInstalledToolchains(scrapHome, *environment);
    if (! installed.has_value()) {
        std::cerr << renderCannotListToolchains(installed.error());
        return 1;
    }
    const auto listed = listRemoteToolchains(copy->index, *installed, host);
    if (listed.empty()) {
        std::cerr << renderIndexListsNoToolchain();
        return 0;
    }
    const auto rows = all ? listed : installableOnly(listed);
    if (rows.empty() && (! all)) {
        std::cerr << renderNothingInstallableHere(environment->directoryName());
        return 0;
    }
    std::cout << renderRemoteListingTable(rows, copy->fetchedAt, all);
    return 0;
}

}  // anonymous namespace

int ToolchainListCommandHandler::execute(const InvocationContext& ctx)
{
    const RuntimeEnvironment& env = *ctx.env;
    const bool remote = isGiven(ctx.options, RemoteOption);
    const bool all = isGiven(ctx.options, AllOption);
    if (all && (! remote)) {
        std::cerr << renderAllWithoutRemote();
        return 1;
    }
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    return remote ? listRemote(env, *scrapHome, all) : listInstalled(env, *scrapHome);
}

}  // namespace scrap::Command
