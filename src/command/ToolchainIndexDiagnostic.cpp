#include "command/ToolchainIndexDiagnostic.h"

#include "command/FetchFailureHint.h"
#include "command/PrintableText.h"
#include "install/IndexCopy.h"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

namespace {

std::string cannotFetchIndex(const std::string_view reason)
{
    return "error: cannot fetch the toolchain index: " + printableText(reason) + "\n";
}

std::string render(const Install::IndexFormatUnsupported& fault)
{
    return "error: the toolchain index has the format " + printableText(fault.format) + ", which this scrap cannot read\nhint: update scrap and run the command again\n";
}

std::string render(const Install::IndexUnavailable& fault)
{
    const std::string text = cannotFetchIndex(fault.reason);
    if (fault.fetchError.has_value()) {
        return text + hintForFetchError(*fault.fetchError);
    }
    return text + std::string{ RetryHint };
}

}  // anonymous namespace

std::string fetchTimeText(const std::optional<std::string>& fetchedAt)
{
    return fetchedAt.value_or("an unknown time");
}

std::string renderIndexFault(const Install::IndexFault& fault)
{
    return std::visit([](const auto& alternative) -> std::string {
        return render(alternative);
    }, fault);
}

std::string renderCopyKept(const std::string_view reason, const std::string_view fetchedAt)
{
    return cannotFetchIndex(reason) + "hint: the copy fetched at " + std::string{ fetchedAt } + " is kept as it was\n";
}

}  // namespace scrap::Command
