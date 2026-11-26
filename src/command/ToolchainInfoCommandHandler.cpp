#include "command/ToolchainInfoCommandHandler.h"

#include "command/InstallingHostOfScrap.h"
#include "command/InvocationContext.h"
#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "command/SavedStatusWarning.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDetails.h"
#include "command/ToolchainDetailsText.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainIndexDiagnostic.h"
#include "command/ToolchainIndexLocation.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainListingFacts.h"
#include "install/IndexCopy.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainSpecifier.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

/**
 * How a listing here marks the toolchain named @p name, or the error lines
 * when the installed toolchains cannot be listed.
 */
std::expected<ToolchainSelection, std::string> selectionHere(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome, std::string_view name)
{
    const auto facts = readListingFacts(env, scrapHome);
    if (! facts.has_value()) {
        return std::unexpected(facts.error());
    }
    return selectionIn(listToolchains(*facts), name);
}

/**
 * Whether @p written, a name as a listing has it, leads in @p index to the
 * toolchain named @p name, as install would choose it on @p host.
 */
bool leadsTo(const Toolchain::ToolchainIndex& index, const Toolchain::InstallingHost& host, std::string_view written, std::string_view name)
{
    const auto choice = Toolchain::parseToolchainChoice(written);
    if (! choice.has_value()) {
        return false;
    }
    const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&*choice);
    if (specifier == nullptr) {
        return false;
    }
    const auto found = Toolchain::lookUpInIndex(index, *specifier, host);
    return found.has_value() && (found->toolchain.text() == name);
}

int tellSystem(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome)
{
    auto compiler = Toolchain::detectSystemCompiler(env.preferredCompiler, env.systemSearchPaths);
    if (! compiler.has_value()) {
        const std::string& requested = compiler.error().requested;
        std::cerr << (requested.empty() ? renderNoCompilerFound() : renderUnusableCompilerRequest(requested));
        return 1;
    }
    auto selection = selectionHere(env, scrapHome, "system");
    if (! selection.has_value()) {
        std::cerr << selection.error();
        return 1;
    }
    std::cout << renderToolchainDetails(SystemDetails{ .compiler = std::move(*compiler), .selection = *selection });
    return 0;
}

int tellInstalled(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome, std::string_view text, const Toolchain::InstalledToolchain& installed)
{
    auto record = Toolchain::usableInstallRecord(installed.directory);
    if (! record.has_value()) {
        std::cerr << renderUnusableInstalled(text, record.error());
        return 1;
    }
    std::optional<Toolchain::InstallRecord>& held = *record;
    if (! held.has_value()) {
        std::cerr << renderNotInstalled(text);
        return 1;
    }
    auto selection = selectionHere(env, scrapHome, installed.name());
    if (! selection.has_value()) {
        std::cerr << selection.error();
        return 1;
    }
    const InstalledDetails details{ .name = installed.name(), .directory = installed.directory, .record = std::move(*held), .selection = *selection };
    std::cout << renderToolchainDetails(details);
    std::cerr << SavedStatusWarning::read(env, scrapHome).linesFor(installed.family, installed.version);
    if (const auto hint = renderUnrecordedHint(details); hint.has_value()) {
        std::cerr << *hint;
    }
    return 0;
}

int tellIndexed(const RuntimeEnvironment& env,
                const std::filesystem::path& scrapHome,
                const Toolchain::HostEnvironment& environment,
                std::string_view text,
                const Toolchain::ToolchainSpecifier& specifier)
{
    const Toolchain::InstallingHost host = installingHostOfScrap(environment);
    auto saved = Install::readSavedIndex(scrapHome, toolchainIndexUrl(env), host.scrapVersion);
    if (! saved.has_value()) {
        std::cerr << renderIndexFault(Install::IndexFault{ std::move(saved.error()) });
        return 1;
    }
    std::optional<Install::SavedIndex>& copy = *saved;
    if (! copy.has_value()) {
        std::cerr << "error: '" << printableTypedEcho(text) << "' is not installed, and the toolchain index has not been fetched\n"
                  << "hint: run 'scrap toolchain update' to fetch it\n";
        return 1;
    }
    auto found = Toolchain::lookUpInIndex(copy->index, specifier, host);
    if (! found.has_value()) {
        std::cerr << "error: '" << printableTypedEcho(text) << "' is neither installed nor in the toolchain index\n"
                  << "hint: run 'scrap toolchain update' to fetch the latest index\n";
        return 1;
    }
    const auto facts = readListingFacts(env, scrapHome);
    if (! facts.has_value()) {
        std::cerr << facts.error();
        return 1;
    }
    const std::string name = found->toolchain.text();
    const Toolchain::ToolchainIndex& index = copy->index;
    const ToolchainSelection selection = selectionOfNotInstalled(listToolchains(*facts), [&index, &host, &name](std::string_view written) {
        return leadsTo(index, host, written, name);
    });
    std::cout << renderToolchainDetails(IndexedDetails{ .lookup = std::move(*found), .fetchedAt = std::move(copy->fetchedAt), .selection = selection });
    return 0;
}

}  // anonymous namespace

int ToolchainInfoCommandHandler::execute(const InvocationContext& ctx)
{
    const RuntimeEnvironment& env = *ctx.env;
    // The parser requires the specifier, so the first positional is there.
    const std::string text = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();
    const auto choice = Toolchain::parseToolchainChoice(text);
    if (! choice.has_value()) {
        std::cerr << renderNotASpecifier(text, choice.error());
        return 1;
    }
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&*choice);
    if (specifier == nullptr) {
        return tellSystem(env, *scrapHome);
    }
    const auto environment = Toolchain::hostEnvironment();
    if (! environment.has_value()) {
        std::cerr << renderNoToolchainForHost(ToolchainNamedIn::CommandLine);
        return 1;
    }

    const auto installed = Toolchain::findInstalledToolchain(*scrapHome, *environment, *specifier);
    if (! installed.has_value()) {
        std::cerr << renderInstalledToolchainFault(text, ToolchainNamedIn::CommandLine, installed.error());
        return 1;
    }
    if (const std::optional<Toolchain::InstalledToolchain>& found = *installed; found.has_value()) {
        return tellInstalled(env, *scrapHome, text, *found);
    }
    return tellIndexed(env, *scrapHome, *environment, text, *specifier);
}

}  // namespace scrap::Command
