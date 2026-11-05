#include "command/ToolchainInstallCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ParsedOptions.h"
#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "command/ToolchainInstallDiagnostic.h"
#include "fetch/CertificateBundle.h"
#include "install/IndexCopy.h"
#include "install/IngotInstaller.h"
#include "shared/constants/version.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/HostGlibc.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainSpecifier.h"
#include "toolchain/ToolchainVersion.h"

#include <cstddef>
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

constexpr std::string_view OfficialIndexUrl = "https://raw.githubusercontent.com/skipbit/scrap-toolchain/main/index.toml";

/**
 * The version of this scrap, read from its banner: "scrap 0.1.0" or
 * "scrap 0.1.0 (v0.1.0-2-g2a761c0)". A banner that holds none gives the lowest
 * version, which then meets no requirement of the index.
 */
Toolchain::ToolchainVersion runningVersion()
{
    const std::string banner = version();
    const std::size_t start = banner.find(' ') + 1;
    const std::string text = banner.substr(start, banner.find(' ', start) - start);
    return Toolchain::ToolchainVersion::parse(text).value_or(Toolchain::ToolchainVersion::lowest());
}

bool asksToReplace(const ParsedOptions& options)
{
    const auto option = options.named.find(std::string{ ForceOption });
    if (option == options.named.end()) {
        return false;
    }
    const bool* const force = std::get_if<bool>(&option->second);
    return (force != nullptr) && *force;
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
    const std::string indexUrl = env.toolchainIndexUrl.empty() ? std::string{ OfficialIndexUrl } : env.toolchainIndexUrl;
    const Toolchain::ToolchainVersion scrapVersion = runningVersion();
    const auto obtained = Install::obtainIndex(scrapHome, Install::IndexSource{ .url = indexUrl, .certificateBundle = bundle }, scrapVersion);
    if (! obtained.has_value()) {
        return std::unexpected(renderIndexFault(obtained.error()));
    }
    const std::optional<std::string>& copyFetchedAt = obtained->copyFetchedAt;
    if (copyFetchedAt.has_value()) {
        std::cerr << "warning: cannot fetch the toolchain index; using the copy fetched at " << *copyFetchedAt << "\n";
    }
    const Toolchain::InstallingHost host{ .environment = environment, .scrapVersion = scrapVersion, .glibcVersion = Toolchain::hostGlibcVersion() };
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

    const std::string name = selected->family + "@" + selected->version.text();
    const std::filesystem::path directory = Toolchain::toolchainDirectory(*scrapHome, selected->family, selected->version.text(), environment->directoryName());
    std::optional<Toolchain::InstallRecord> installed;
    if (! asksToReplace(ctx.options)) {
        auto read = Toolchain::usableInstallRecord(directory);
        if (! read.has_value()) {
            std::cerr << renderUnusableInstalled(text, read.error());
            return 1;
        }
        installed = std::move(*read);
    }
    if (installed.has_value()) {
        reportInstalled(name, directory, *installed, Toolchain::integrityOf(selected->ingot));
    } else {
        const auto done = Install::installIngot({ .scrapHome = *scrapHome,
                                                  .selected = *selected,
                                                  .environment = *environment,
                                                  .certificateBundle = bundle,
                                                  .announce = [](const std::string_view step) {
            std::cerr << step << "\n";
        } });
        if (! done.has_value()) {
            std::cerr << renderInstallFailure(text, done.error());
            return 1;
        }
        std::cerr << "Installed '" << name << "' at '" << printablePath(*done) << "'\n";
    }
    return selectWhenNoneIs(*scrapHome, *current, name) ? 0 : 1;
}

}  // namespace scrap::Command
