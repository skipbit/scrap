#include "command/StatusWarningText.h"

#include "command/PrintableText.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/StatusWarning.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <string>

namespace scrap::Command {

std::string anotherVersionHint(const std::string& name)
{
    return "hint: install another version, such as '" + printableText(name) + "'\n";
}

std::string renderStatusWarning(const Toolchain::StatusWarning& warning)
{
    const bool deprecated = (warning.status == Toolchain::ToolchainStatus::Deprecated);
    std::string rendered = "warning: '" + printableText(warning.name) + (deprecated ? "' is deprecated\n" : "' is no longer provided\n");
    if (warning.alternative.has_value()) {
        rendered += deprecated ? "hint: install a newer version, such as '" + printableText(*warning.alternative) + "'\n" : anotherVersionHint(*warning.alternative);
    }
    return rendered;
}

std::string renderStatusWarningOf(const Toolchain::ToolchainIndex& index, const std::string& family, const Toolchain::ToolchainVersion& version, const Toolchain::InstallingHost& host)
{
    const auto warning = Toolchain::statusWarningFor(index, family, version, host);
    return warning.has_value() ? renderStatusWarning(*warning) : std::string{};
}

}  // namespace scrap::Command
