#include "command/ToolchainUpdateCommandHandler.h"

#include "command/IndexUpdateReport.h"
#include "command/InstallingHostOfScrap.h"
#include "command/InvocationContext.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainIndexDiagnostic.h"
#include "command/ToolchainIndexLocation.h"
#include "fetch/CertificateBundle.h"
#include "install/IndexCopy.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IndexChanges.h"
#include "toolchain/IngotSelection.h"

#include <expected>  // IWYU pragma: keep
#include <iostream>
#include <optional>
#include <string>
#include <variant>

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
    const Toolchain::InstallingHost host = installingHostOfScrap(*environment);
    // Read before fetching, which replaces it.
    const auto saved = Install::readSavedIndex(*scrapHome, url, host.scrapVersion);
    const auto bundle = Fetch::findCertificateBundle(env.certificateFile, Fetch::knownCertificateBundles());
    const auto fetched = Install::fetchIndex(*scrapHome, Install::IndexSource{ .url = url, .certificateBundle = bundle }, host.scrapVersion);
    // A copy this scrap cannot read has nothing to compare with, and an update
    // never falls back on the copy, so its format is no cause of a failure.
    const std::optional<Install::SavedIndex> before = saved.value_or(std::nullopt);
    if (! fetched.has_value()) {
        const auto* unavailable = std::get_if<Install::IndexUnavailable>(&fetched.error());
        if ((unavailable != nullptr) && before.has_value()) {
            std::cerr << renderCopyKept(*unavailable, fetchTimeText(before->fetchedAt));
        } else {
            std::cerr << renderIndexFault(fetched.error());
        }
        return 1;
    }

    const std::string fetchedAt = fetchTimeText(fetched->fetchedAt);
    if (before.has_value()) {
        std::cout << renderIndexChanges(fetchedAt, environment->directoryName(), Toolchain::compareIndexes(before->index, fetched->index, host));
    } else {
        std::cout << renderFirstIndex(fetchedAt, environment->directoryName(), Toolchain::countInstallable(fetched->index, host));
    }
    return 0;
}

}  // namespace scrap::Command
