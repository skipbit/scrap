#include "command/ToolchainSelectCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/ScrapHome.h"
#include "command/ToolchainDiagnostic.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainSpecifier.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

/**
 * What to keep as the global default for @p specifier: the newest
 * installed toolchain it names, written in full.
 */
std::expected<std::string, std::string> installedChoice(const std::filesystem::path& scrapHome, const std::string& text, const Toolchain::ToolchainSpecifier& specifier)
{
    const auto installed = Toolchain::findInstalledToolchainForHost(scrapHome, specifier);
    if (! installed.has_value()) {
        return std::unexpected(renderInstalledToolchainFault(text, installed.error()));
    }
    return installed->name();
}

}  // anonymous namespace

int ToolchainSelectCommandHandler::execute(const InvocationContext& ctx)
{
    // The parser requires the specifier, so the first positional is there.
    const std::string text = ctx.options.positional.empty() ? std::string{} : ctx.options.positional.front();
    const auto choice = Toolchain::parseToolchainChoice(text);
    if (! choice.has_value()) {
        std::cerr << renderNotASpecifier(text, choice.error());
        return 1;
    }

    const auto scrapHome = requireScrapHome(*ctx.env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }

    std::string selected = "system";
    if (const auto* specifier = std::get_if<Toolchain::ToolchainSpecifier>(&*choice)) {
        auto installed = installedChoice(*scrapHome, text, *specifier);
        if (! installed.has_value()) {
            std::cerr << installed.error();
            return 1;
        }
        selected = std::move(*installed);
    }

    const std::filesystem::path file = Toolchain::globalConfigFile(*scrapHome);
    if (auto written = Toolchain::writeGlobalDefaultFile(file, selected); ! written.has_value()) {
        std::cerr << renderGlobalConfigFileError(file, written.error());
        return 1;
    }
    std::cerr << "Selected '" << selected << "' as the global default\n";
    return 0;
}

}  // namespace scrap::Command
