#include "command/ToolchainInstallDiagnostic.h"

#include "command/FetchFailureHint.h"
#include "command/PrintableText.h"
#include "command/StatusWarningText.h"
#include "fetch/FetchErrorText.h"
#include "install/IngotInstaller.h"
#include "install/IngotUnpack.h"
#include "toolchain/IngotSelection.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

constexpr std::size_t ExamplesShown = 3;

std::string firstFew(const std::vector<std::string>& names)
{
    std::string joined;
    for (std::size_t i = 0; i < std::min(names.size(), ExamplesShown); ++i) {
        if (i > 0) {
            joined += ", ";
        }
        joined += printableText(names[i]);
    }
    return joined;
}

std::string errorLine(const std::string_view specifier, const std::string_view rest)
{
    std::string line = "error: '" + printableTypedEcho(specifier) + "' ";
    line += rest;
    line += '\n';
    return line;
}

std::string render(const std::string_view specifier, const Toolchain::NoMatchInIndex& fault)
{
    const std::string text = "error: no toolchain matches '" + printableTypedEcho(specifier) + "'\n";
    if (! fault.versions.empty()) {
        return text + "hint: available versions include " + firstFew(fault.versions) + "\n";
    }
    if (! fault.families.empty()) {
        return text + "hint: available toolchains include " + firstFew(fault.families) + "\n";
    }
    return text + "hint: the toolchain index lists nothing to install\n";
}

std::string render(const std::string_view specifier, const Toolchain::NoIngotForHost& fault)
{
    const std::string text = errorLine(specifier, "has no build for " + fault.environment);
    if (fault.versions.empty()) {
        return text + "hint: no version of it is built for this host\n";
    }
    return text + "hint: available versions for this host include " + firstFew(fault.versions) + "\n";
}

std::string render(const std::string_view specifier, const Toolchain::NoLongerOffered& fault)
{
    const std::string text = errorLine(specifier, "is no longer provided");
    if (! fault.alternative.has_value()) {
        return text + "hint: no other version of it can be installed on this host\n";
    }
    return text + anotherVersionHint(fault.family + "@" + *fault.alternative);
}

std::string render(const std::string_view specifier, const Toolchain::NeedsNewerGlibc& fault)
{
    return errorLine(specifier, "needs glibc " + printableText(fault.required) + " or later; this host has " + printableText(fault.host))
           + "hint: install an older version, or use a newer host\n";
}

std::string render(const std::string_view specifier, const Toolchain::NeedsNewerScrap& fault)
{
    return errorLine(specifier, "needs scrap " + printableText(fault.required) + " or later") + "hint: update scrap and run the command again\n";
}

std::string cannotInstall(const std::string_view specifier, const std::string_view reason, const std::string_view hint)
{
    std::string text = "error: cannot install '" + printableTypedEcho(specifier) + "': " + printableText(reason) + "\n";
    text += hint;
    return text;
}

std::string render(const std::string_view specifier, const Install::IngotNotFetched& failure)
{
    return cannotInstall(specifier, Fetch::describeFetchError(failure.error), hintForFetchError(failure.error));
}

std::string render(const std::string_view specifier, const Install::IngotDoesNotMatch&)
{
    return "error: the download of '" + printableTypedEcho(specifier) + "' does not match the index\n" + std::string{ RetryHint };
}

std::string render(const std::string_view specifier, const Install::RegistryNotLocated& failure)
{
    return cannotInstall(specifier, "the index names the registry '" + failure.registry + "' and the tag '" + failure.tag + "', which do not say where its build is kept", RetryHint);
}

std::string render(const std::string_view specifier, const Install::RegistryAnswerUnusable& failure)
{
    return cannotInstall(specifier, "the registry answered '" + failure.url + "' with what scrap cannot use: " + failure.reason, RetryHint);
}

std::string render(const std::string_view specifier, const Install::IngotNotUnpacked& failure)
{
    return cannotInstall(specifier, failure.reason, hintForWriteFailure(failure.code));
}

std::string render(const std::string_view specifier, const Install::IngotNotPlaced& failure)
{
    std::string hint = hintForWriteFailure(failure.code);
    if (failure.previousKeptAt.has_value()) {
        hint = "hint: the toolchain that was installed is at '" + printablePath(*failure.previousKeptAt) + "'; move it back to '" + printablePath(failure.path)
               + "', since a later install removes it once it is a day old\n";
    }
    return cannotInstall(specifier, "cannot write '" + printablePath(failure.path) + "': " + failure.code.message(), hint);
}

}  // anonymous namespace

std::string renderSelectionFault(const std::string_view specifier, const Toolchain::IngotSelectionFault& fault)
{
    return std::visit([specifier](const auto& alternative) -> std::string {
        return render(specifier, alternative);
    }, fault);
}

std::string renderInstallFailure(const std::string_view specifier, const Install::IngotInstallFailure& failure)
{
    return std::visit([specifier](const auto& alternative) -> std::string {
        return render(specifier, alternative);
    }, failure);
}

}  // namespace scrap::Command
