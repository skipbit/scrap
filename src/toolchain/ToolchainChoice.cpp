#include "toolchain/ToolchainChoice.h"

#include "toolchain/ToolchainSpecifier.h"

#include <string>
#include <string_view>

namespace scrap::Toolchain {

std::expected<ToolchainChoice, std::string> parseToolchainChoice(std::string_view text)
{
    if (text == "system") {
        return SystemToolchain{};
    }
    return parseToolchainSpecifier(text);
}

}  // namespace scrap::Toolchain
