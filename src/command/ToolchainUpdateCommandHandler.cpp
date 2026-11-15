#include "command/ToolchainUpdateCommandHandler.h"

#include "command/IndexUpdateReport.h"
#include "command/InvocationContext.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainIndexLocation.h"
#include "command/ToolchainInstallDiagnostic.h"
#include "fetch/CertificateBundle.h"
#include "install/IndexCopy.h"
#include "install/UtcTime.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/HostGlibc.h"
#include "toolchain/IndexChanges.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/ToolchainVersion.h"

#include <chrono>
#include <expected>  // IWYU pragma: keep
#include <iostream>
#include <optional>
#include <string>

namespace scrap::Command {

int ToolchainUpdateCommandHandler::execute(const InvocationContext& ctx)
{
    const RuntimeEnvironment& env = *ctx.env;
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    const auto environment = Toolchain::hostEnvironment();
    if (! environment.has_value()) {
        std::cerr << renderNoToolchainForHost(ToolchainNamedIn::CommandLine);
        return 1;
    }

    const std::string url = toolchainIndexUrl(env);
    const Toolchain::ToolchainVersion scrapVersion = runningScrapVersion();
    // Read before fetching, which replaces it. A copy this scrap cannot read
    // has nothing to compare with.
    const std::optional<Install::SavedIndex> before = Install::readSavedIndex(*scrapHome, url, scrapVersion).value_or(std::nullopt);
    const auto now = std::chrono::system_clock::now();
    const auto bundle = Fetch::findCertificateBundle(env.certificateFile, Fetch::knownCertificateBundles());
    const auto obtained = Install::obtainIndex(*scrapHome, Install::IndexSource{ .url = url, .certificateBundle = bundle }, scrapVersion, now);
    if (! obtained.has_value()) {
        std::cerr << renderIndexFault(obtained.error());
        return 1;
    }
    if (const std::optional<Install::CopyStandingIn>& standIn = obtained->standIn; standIn.has_value()) {
        std::cerr << renderCopyKept(standIn->reason, standIn->fetchedAt);
        return 1;
    }

    const Toolchain::InstallingHost host{ .environment = *environment, .scrapVersion = scrapVersion, .glibcVersion = Toolchain::hostGlibcVersion() };
    const std::string fetchedAt = Install::utcText(now).value_or("an unknown time");
    if (before.has_value()) {
        std::cout << renderIndexChanges(fetchedAt, environment->directoryName(), Toolchain::compareIndexes(before->index, obtained->index, host));
    } else {
        std::cout << renderFirstIndex(fetchedAt, environment->directoryName(), Toolchain::countInstallable(obtained->index, host));
    }
    return 0;
}

}  // namespace scrap::Command
