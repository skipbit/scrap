#include "command/ToolchainListingFacts.h"

#include "command/RuntimeEnvironment.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainNaming.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

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
    auto above = namedAboveGlobalDefaultHere(env);
    if (! above.has_value()) {
        std::cerr << asWarning(std::move(above.error()));
        return NameHereUnreadable{};
    }
    if (! above->inProject) {
        return std::nullopt;
    }
    if (const std::optional<NamedToolchain>& named = above->named; named.has_value()) {
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

std::expected<ListingFacts, std::string> readListingFacts(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome)
{
    auto installed = installedForHost(scrapHome);
    if (! installed.has_value()) {
        return std::unexpected(renderCannotListToolchains(installed.error()));
    }
    auto systemCompiler = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    if ((! systemCompiler.has_value()) && (! systemCompiler.error().requested.empty())) {
        std::cerr << asWarning(renderUnusableCompilerRequest(systemCompiler.error().requested));
    }
    const auto globalDefault = namedGlobalDefault(env);
    if (! globalDefault.has_value()) {
        std::cerr << asWarning(globalDefault.error());
    }
    auto here = readNamedHere(env, globalDefault);

    return ListingFacts{ .installed = std::move(*installed),
                         .systemCompiler = systemCompiler.has_value() ? std::optional{ std::move(*systemCompiler) } : std::nullopt,
                         .globalDefault = globalDefault.value_or(std::nullopt),
                         .here = std::move(here) };
}

}  // namespace scrap::Command
