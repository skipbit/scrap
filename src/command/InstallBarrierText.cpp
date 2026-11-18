#include "command/InstallBarrierText.h"

#include "command/PrintableText.h"
#include "toolchain/IngotSelection.h"

#include <string>
#include <variant>

namespace scrap::Command {

namespace {

std::string reasonText(const Toolchain::NoMatchInIndex& /*barrier*/)
{
    return "not in the toolchain index";
}

std::string reasonText(const Toolchain::NoIngotForHost& barrier)
{
    return "not built for " + printableText(barrier.environment);
}

std::string reasonText(const Toolchain::NoLongerOffered& /*barrier*/)
{
    return "no longer provided";
}

std::string reasonText(const Toolchain::NeedsNewerGlibc& barrier)
{
    return "needs glibc " + printableText(barrier.required) + " or later; this host has " + printableText(barrier.host);
}

std::string reasonText(const Toolchain::NeedsNewerScrap& barrier)
{
    return "needs scrap " + printableText(barrier.required) + " or later";
}

}  // anonymous namespace

std::string renderInstallBarrier(const Toolchain::IngotSelectionFault& barrier)
{
    return std::visit([](const auto& alternative) -> std::string {
        return reasonText(alternative);
    }, barrier);
}

}  // namespace scrap::Command
