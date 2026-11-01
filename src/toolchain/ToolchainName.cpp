#include "toolchain/ToolchainName.h"

#include <algorithm>
#include <string_view>

namespace scrap::Toolchain {

namespace {

bool isNameCharacter(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '-') || (c == '_') || (c == '.') || (c == '+');
}

}  // anonymous namespace

bool isToolchainName(std::string_view text)
{
    return (! text.empty()) && (text != ".") && (text != "..") && std::ranges::all_of(text, isNameCharacter);
}

}  // namespace scrap::Toolchain
