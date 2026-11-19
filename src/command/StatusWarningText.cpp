#include "command/StatusWarningText.h"

#include "command/PrintableText.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/StatusWarning.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <string>

namespace scrap::Command {

namespace {

std::string statusPhrase(const Toolchain::ToolchainStatus status)
{
    switch (status) {
    case Toolchain::ToolchainStatus::Active:
        return {};
    case Toolchain::ToolchainStatus::Deprecated:
        return "is deprecated";
    case Toolchain::ToolchainStatus::Disabled:
        return "is no longer provided";
    }
    return {};
}

std::string hintOf(const Toolchain::ToolchainStatus status, const std::string& alternative)
{
    switch (status) {
    case Toolchain::ToolchainStatus::Active:
        return {};
    case Toolchain::ToolchainStatus::Deprecated:
        return "hint: install a newer version, such as '" + printableText(alternative) + "'\n";
    case Toolchain::ToolchainStatus::Disabled:
        return anotherVersionHint(alternative);
    }
    return {};
}

}  // anonymous namespace

std::string anotherVersionHint(const std::string& name)
{
    return "hint: install another version, such as '" + printableText(name) + "'\n";
}

std::string renderStatusWarning(const Toolchain::StatusWarning& warning)
{
    if (warning.status == Toolchain::ToolchainStatus::Active) {
        return {};
    }
    std::string rendered = "warning: '" + printableText(warning.name) + "' " + statusPhrase(warning.status) + "\n";
    if (warning.alternative.has_value()) {
        rendered += hintOf(warning.status, *warning.alternative);
    }
    return rendered;
}

std::string renderStatusWarningOf(const Toolchain::ToolchainIndex& index, const std::string& family, const Toolchain::ToolchainVersion& version, const Toolchain::InstallingHost& host)
{
    const auto warning = Toolchain::statusWarningFor(index, family, version, host);
    return warning.has_value() ? renderStatusWarning(*warning) : std::string{};
}

}  // namespace scrap::Command
