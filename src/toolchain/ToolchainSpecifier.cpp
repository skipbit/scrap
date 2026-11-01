#include "toolchain/ToolchainSpecifier.h"

#include "toolchain/ToolchainName.h"
#include "toolchain/ToolchainVersion.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Toolchain {

bool ToolchainSpecifier::matches(std::string_view family, const ToolchainVersion& version) const
{
    return (this->family == family) && ((! this->version.has_value()) || version.startsWith(*this->version));
}

std::expected<ToolchainSpecifier, std::string> parseToolchainSpecifier(std::string_view text)
{
    if (text.empty()) {
        return std::unexpected("it is empty");
    }
    const std::size_t at = text.find('@');
    if (at == 0) {
        return std::unexpected("it names no family before '@'");
    }
    const std::string_view family = text.substr(0, at);
    if (! isToolchainName(family)) {
        return std::unexpected("its family holds a character other than letters, digits, '-', '_', '.' and '+', or is '.' or '..'");
    }
    if (at == std::string_view::npos) {
        return ToolchainSpecifier{ .family = std::string{ family }, .version = std::nullopt };
    }
    const std::string_view versionText = text.substr(at + 1);
    if (versionText.empty()) {
        return std::unexpected("it names no version after '@'");
    }
    if (versionText.contains('@')) {
        return std::unexpected("it has more than one '@'");
    }
    std::optional<ToolchainVersion> version = ToolchainVersion::parse(versionText);
    if (! version.has_value()) {
        return std::unexpected("its version is not numbers joined by dots");
    }
    return ToolchainSpecifier{ .family = std::string{ family }, .version = std::move(version) };
}

}  // namespace scrap::Toolchain
