#include "command/ToolchainInstallDiagnostic.h"

#include "command/PrintableText.h"
#include "command/WriteFailureHint.h"
#include "fetch/FetchErrorText.h"
#include "fetch/UrlFetch.h"
#include "install/IndexCopy.h"
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
constexpr std::string_view NetworkHint = "hint: check the network connection and run the command again\n";
constexpr std::string_view CertificateHint = "hint: install the root certificates of the system, such as the ca-certificates package, or set SSL_CERT_FILE to a file holding them\n";
constexpr std::string_view RetryHint = "hint: run the command again; if it keeps failing, report it\n";

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

/**
 * The hint for a failure the operating system reported as @p code.
 */
std::string hintFor(const std::error_code& code)
{
    return std::string{ sharedWriteHint(code).value_or(RetryHint) };
}

std::string hintFor(const Fetch::UrlRefused&)
{
    return std::string{ RetryHint };
}

std::string hintFor(const Fetch::NoCertificateBundle&)
{
    return std::string{ CertificateHint };
}

std::string hintFor(const Fetch::CertificateFailure&)
{
    return std::string{ CertificateHint };
}

std::string hintFor(const Fetch::TransferFailure&)
{
    return std::string{ NetworkHint };
}

std::string hintFor(const Fetch::UnexpectedStatus&)
{
    return std::string{ NetworkHint };
}

std::string hintFor(const Fetch::TooManyRedirects&)
{
    return std::string{ NetworkHint };
}

std::string hintFor(const Fetch::ContentTooLarge&)
{
    return std::string{ RetryHint };
}

std::string hintFor(const Fetch::CannotWriteDestination& error)
{
    return hintFor(error.code);
}

std::string hintForFetchError(const Fetch::FetchError& error)
{
    return std::visit([](const auto& alternative) -> std::string {
        return hintFor(alternative);
    }, error);
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
    return text + "hint: install another version, such as '" + printableText(fault.family) + "@" + printableText(*fault.alternative) + "'\n";
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

std::string render(const std::string_view specifier, const Install::RegistryNotSupported&)
{
    return cannotInstall(specifier, "its build is kept in a registry, which this scrap cannot fetch from", "hint: update scrap and run the command again\n");
}

std::string render(const std::string_view specifier, const Install::IngotNotUnpacked& failure)
{
    return cannotInstall(specifier, failure.reason, hintFor(failure.code));
}

std::string render(const std::string_view specifier, const Install::IngotNotPlaced& failure)
{
    return cannotInstall(specifier, "cannot write '" + printablePath(failure.path) + "': " + failure.code.message(), hintFor(failure.code));
}

std::string render(const Install::IndexFormatUnsupported& fault)
{
    return "error: the toolchain index has the format " + printableText(fault.format) + ", which this scrap cannot read\nhint: update scrap and run the command again\n";
}

std::string render(const Install::IndexUnavailable& fault)
{
    const std::string text = "error: cannot fetch the toolchain index: " + printableText(fault.reason) + "\n";
    if (fault.fetchError.has_value()) {
        return text + hintForFetchError(*fault.fetchError);
    }
    return text + std::string{ RetryHint };
}

}  // anonymous namespace

std::string renderNotInstallable(const std::string_view text, const std::string_view reason)
{
    std::string rendered = "error: '" + printableTypedEcho(text) + "' is not a toolchain specifier: ";
    rendered += reason;
    rendered += "\nhint: write it as <family>[@<version>], such as 'gcc@15'\n";
    return rendered;
}

std::string renderSelectionFault(const std::string_view specifier, const Toolchain::IngotSelectionFault& fault)
{
    return std::visit([specifier](const auto& alternative) -> std::string {
        return render(specifier, alternative);
    }, fault);
}

std::string renderIndexFault(const Install::IndexFault& fault)
{
    return std::visit([](const auto& alternative) -> std::string {
        return render(alternative);
    }, fault);
}

std::string renderInstallFailure(const std::string_view specifier, const Install::IngotInstallFailure& failure)
{
    return std::visit([specifier](const auto& alternative) -> std::string {
        return render(specifier, alternative);
    }, failure);
}

}  // namespace scrap::Command
