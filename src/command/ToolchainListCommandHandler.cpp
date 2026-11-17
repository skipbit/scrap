#include "command/ToolchainListCommandHandler.h"

#include "command/InvocationContext.h"
#include "command/RuntimeEnvironment.h"
#include "command/ScrapHome.h"
#include "command/ToolchainListing.h"
#include "command/ToolchainListingFacts.h"
#include "command/ToolchainListingTable.h"

#include <expected>  // IWYU pragma: keep
#include <iostream>

namespace scrap::Command {

int ToolchainListCommandHandler::execute(const InvocationContext& ctx)
{
    const RuntimeEnvironment& env = *ctx.env;
    const auto scrapHome = requireScrapHome(env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    auto facts = readListingFacts(env, *scrapHome);
    if (! facts.has_value()) {
        std::cerr << facts.error();
        return 1;
    }
    const ToolchainListing listing = listToolchains(*facts);
    if (listing.toolchains.empty()) {
        std::cerr << "no toolchain is installed\n"
                     "hint: run 'scrap toolchain install <toolchain>', such as 'gcc'\n";
        return 0;
    }
    std::cout << renderToolchainListingTable(listing);
    return 0;
}

}  // namespace scrap::Command
