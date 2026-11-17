#include "command/ToolchainInstallCommandHandler.h"

#include "command/InstallingHostOfScrap.h"
#include "command/InvocationContext.h"
#include "command/ParsedOptions.h"
#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainIndexDiagnostic.h"
#include "command/ToolchainIndexLocation.h"
#include "command/ToolchainInstallDiagnostic.h"
#include "fetch/CertificateBundle.h"
#include "install/IndexCopy.h"
#include "install/IngotInstaller.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

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

bool asksToReplace(const ParsedOptions& options)
{
    return isGiven(options, ForceOption);
}

/**
 * Make @p name the global default when there is none, which @p current says.
 *
 * @return Whether it went without a failure, which was then reported.
 */
bool selectWhenNoneIs(const std::filesystem::path& scrapHome, const std::optional<std::string>& current, const std::string& name)
{
    if (current.has_value()) {
        return true;
    }
    const std::filesystem::path file = Toolchain::globalConfigFile(scrapHome);
    if (auto written = Toolchain::writeGlobalDefaultFile(file, name); ! written.has_value()) {
        std::cerr << renderGlobalConfigFileError(file, written.error());
        return false;
    }
    std::cerr << "Selected '" << name << "' as the global default, since no toolchain was selected\n";
    return true;
}

void sayInstalled(const std::string& name, const std::filesystem::path& directory)
{
    std::cerr << "'" << name << "' is already installed at '" << printablePath(directory) << "'\n";
}

/**
 * Report that @p name is installed in @p directory, and whether the index now
 * lists another build of it than @p integrity.
 */
void reportInstalled(const std::string& name, const std::filesystem::path& directory, const Toolchain::InstallRecord& record, const std::string& integrity)
{
    sayInstalled(name, directory);
    const std::optional<std::string>& recorded = record.ingotIntegrity;
    if (recorded.has_value() && (*recorded != integrity)) {
        std::cerr << "warning: the index now lists a different build of '" << name << "'\n"
                  << "hint: run 'scrap toolchain install --force " << name << "' to replace it\n";
    }
}

/**
 * The specifier @p text holds, or the report that it names nothing to install.
 */
std::expected<Toolchain::ToolchainSpecifier, std::string> specifierToInstall(const std::string& text)
{
    const auto choice = Toolchain::parseToolchainChoice(text);
    if (! choice.has_value()) {
        return std::unexpected(renderNotInstallable(text, choice.error()));
    }
    const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&*choice);
    if (specifier == nullptr) {
        return std::unexpected(renderNotInstallable(text, "it names the compiler found on the system"));
    }
    return *specifier;
}

/**
 * What the index offers for @p specifier on @p environment, or the report of
 * why it offers nothing.
 */
std::expected<Toolchain::SelectedIngot, std::string> chooseIngot(const RuntimeEnvironment& env,
                                                                 const std::filesystem::path& scrapHome,
                                                                 const Toolchain::HostEnvironment& environment,
                                                                 const std::optional<std::filesystem::path>& bundle,
                                                                 const std::string& text,
                                                                 const Toolchain::ToolchainSpecifier& specifier)
{
    const Toolchain::InstallingHost host = installingHostOfScrap(environment);
    const auto obtained = Install::obtainIndex(scrapHome, Install::IndexSource{ .url = toolchainIndexUrl(env), .certificateBundle = bundle }, host.scrapVersion);
    if (! obtained.has_value()) {
        return std::unexpected(renderIndexFault(obtained.error()));
    }
    const std::optional<Install::CopyStandingIn>& standIn = obtained->standIn;
    if (standIn.has_value()) {
        std::cerr << "warning: " << printableText(standIn->reason) << "\n"
                  << "warning: cannot fetch the toolchain index; using the copy fetched at " << fetchTimeText(standIn->fetchedAt) << "\n";
    }
    auto selected = Toolchain::selectIngot(obtained->index, specifier, host);
    if (! selected.has_value()) {
        return std::unexpected(renderSelectionFault(text, selected.error()));
    }
    return std::move(*selected);
}

/**
 * The name of the toolchain @p specifier names version and all, when that one is
 * installed and can be used, so that what the index says of it does not matter.
 */
std::optional<std::string> reportExactlyInstalled(const std::filesystem::path& scrapHome, const Toolchain::HostEnvironment& environment, const Toolchain::ToolchainSpecifier& specifier)
{
    if (! specifier.version.has_value()) {
        return std::nullopt;
    }
    const std::string version = specifier.version->text();
    const std::filesystem::path directory = Toolchain::toolchainDirectory(scrapHome, specifier.family, version, environment.directoryName());
    const auto record = Toolchain::usableInstallRecord(directory);
    if ((! record.has_value()) || (! record->has_value())) {
        return std::nullopt;
    }
    std::string name = specifier.family + "@" + version;
    sayInstalled(name, directory);
    return name;
}

/**
 * Install @p selected, unless it is there and @p replace does not ask for that.
 *
 * @return Whether that went without a failure, which was then reported.
 */
bool bringIn(const std::filesystem::path& scrapHome,
             const Toolchain::HostEnvironment& environment,
             const std::optional<std::filesystem::path>& bundle,
             const std::string& text,
             const Toolchain::SelectedIngot& selected,
             const bool replace)
{
    const std::string name = selected.family + "@" + selected.version.text();
    const std::filesystem::path directory = Toolchain::toolchainDirectory(scrapHome, selected.family, selected.version.text(), environment.directoryName());
    std::optional<Toolchain::InstallRecord> installed;
    if (! replace) {
        auto read = Toolchain::usableInstallRecord(directory);
        if (! read.has_value()) {
            std::cerr << renderUnusableInstalled(text, read.error());
            return false;
        }
        installed = std::move(*read);
    }
    if (installed.has_value()) {
        reportInstalled(name, directory, *installed, Toolchain::integrityOf(selected.ingot));
        return true;
    }
    const auto done = Install::installIngot({ .scrapHome = scrapHome,
                                              .selected = selected,
                                              .environment = environment,
                                              .certificateBundle = bundle,
                                              .announce = [](const std::string_view step) {
        std::cerr << step << "\n";
    } });
    if (! done.has_value()) {
        std::cerr << renderInstallFailure(text, done.error());
        return false;
    }
    std::cerr << "Installed '" << name << "' at '" << printablePath(*done) << "'\n";
    return true;
}

}  // anonymous namespace

int ToolchainInstallCommandHandler::execute(const InvocationContext& ctx)
{
    // The parser requires the specifier, so the first positional is there.
    const std::string text = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();
    const auto specifier = specifierToInstall(text);
    if (! specifier.has_value()) {
        std::cerr << specifier.error();
        return 1;
    }

    const auto scrapHome = requireScrapHome(*ctx.env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    const std::filesystem::path configFile = Toolchain::globalConfigFile(*scrapHome);
    const auto current = Toolchain::readGlobalDefaultFile(configFile);
    if (! current.has_value()) {
        std::cerr << renderGlobalConfigFileError(configFile, current.error());
        return 1;
    }
    const auto environment = Toolchain::hostEnvironment();
    if (! environment.has_value()) {
        std::cerr << renderNoToolchainForHost(ToolchainNamedIn::CommandLine);
        return 1;
    }

    const auto bundle = Fetch::findCertificateBundle(ctx.env->certificateFile, Fetch::knownCertificateBundles());
    const auto selected = chooseIngot(*ctx.env, *scrapHome, *environment, bundle, text, *specifier);
    if (! selected.has_value()) {
        if (! asksToReplace(ctx.options)) {
            if (const auto name = reportExactlyInstalled(*scrapHome, *environment, *specifier); name.has_value()) {
                return selectWhenNoneIs(*scrapHome, *current, *name) ? 0 : 1;
            }
        }
        std::cerr << selected.error();
        return 1;
    }

    if (! bringIn(*scrapHome, *environment, bundle, text, *selected, asksToReplace(ctx.options))) {
        return 1;
    }
    return selectWhenNoneIs(*scrapHome, *current, selected->family + "@" + selected->version.text()) ? 0 : 1;
}

}  // namespace scrap::Command
