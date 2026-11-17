#include "command/RemoteListingText.h"

#include "command/InstallBarrierText.h"
#include "command/PrintableText.h"
#include "command/RemoteListing.h"
#include "command/TextColumns.h"
#include "command/ToolchainIndexDiagnostic.h"
#include "toolchain/ToolchainIndexParser.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Command {

std::string renderRemoteListingTable(const std::vector<RemoteToolchain>& rows, const std::optional<std::string>& fetchedAt, const bool all)
{
    std::vector<std::vector<std::string>> table;
    table.push_back({ "TOOLCHAIN", "STATUS", "INSTALLED" });
    if (all) {
        table.back().emplace_back("NOTE");
    }
    for (const RemoteToolchain& row : rows) {
        table.push_back({ printableText(row.name), std::string{ Toolchain::statusName(row.status) }, row.installed ? "yes" : "" });
        if (all) {
            table.back().push_back(row.barrier.has_value() ? renderInstallBarrier(*row.barrier) : std::string{});
        }
    }
    std::string rendered = layOutColumns(table);
    rendered += "\nindex fetched at ";
    rendered += printableText(fetchTimeText(fetchedAt));
    rendered += "\n";
    return rendered;
}

std::string renderNothingInstallableHere(const std::string_view environment)
{
    std::string rendered = "no toolchain in the index can be installed on " + printableText(environment);
    rendered += "\nhint: run 'scrap toolchain list --remote --all' to list them all\n";
    return rendered;
}

std::string renderIndexNotFetched()
{
    return "error: the toolchain index has not been fetched\n"
           "hint: run 'scrap toolchain update' to fetch it\n";
}

std::string renderAllWithoutRemote()
{
    return "error: '--all' is used with '--remote'\n"
           "hint: run 'scrap toolchain list --remote --all'\n";
}

}  // namespace scrap::Command
