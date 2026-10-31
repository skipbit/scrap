#include "toolchain/ToolchainSpecifier.h"

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
    if (at == std::string_view::npos) {
        return ToolchainSpecifier{ .family = std::string{ text }, .version = std::nullopt };
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
    return ToolchainSpecifier{ .family = std::string{ text.substr(0, at) }, .version = std::move(version) };
}

}  // namespace scrap::Toolchain
