#include "command/ToolchainUnselectCommandHandler.h"

#include "command/GlobalDefaultFile.h"
#include "command/InvocationContext.h"
#include "command/PrintableText.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace scrap::Command {

int ToolchainUnselectCommandHandler::execute(const InvocationContext& ctx)
{
    const auto scrapHome = requireScrapHome(*ctx.env);
    if (! scrapHome.has_value()) {
        std::cerr << scrapHome.error();
        return 1;
    }
    const std::filesystem::path file = globalConfigFile(*scrapHome);
    const auto read = readGlobalDefault(file);
    if (! read.has_value()) {
        std::cerr << read.error();
        return 1;
    }
    const std::optional<std::string>& selected = *read;
    if (! selected.has_value()) {
        std::cerr << "No toolchain is selected globally\n";
        return 0;
    }
    if (auto written = writeGlobalDefault(file, std::nullopt); ! written.has_value()) {
        std::cerr << written.error();
        return 1;
    }
    std::cerr << "Unselected '" << printableText(*selected) << "'; no toolchain is selected globally\n";
    return 0;
}

}  // namespace scrap::Command
