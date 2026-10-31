#include "toolchain/ToolchainVersion.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Toolchain {

namespace {

/**
 * @p part without its leading zeros, keeping one zero for zero itself.
 */
std::string withoutLeadingZeros(std::string_view part)
{
    const std::size_t first = part.find_first_not_of('0');
    return (first == std::string_view::npos) ? std::string{ "0" } : std::string{ part.substr(first) };
}

/**
 * Compare two parts as numbers. A longer part, having no leading zeros, is
 * the larger number; parts of one length compare digit by digit.
 */
std::strong_ordering compareParts(const std::string& left, const std::string& right)
{
    if (left.size() != right.size()) {
        return left.size() <=> right.size();
    }
    return left.compare(right) <=> 0;
}

bool isDigit(char c)
{
    return (c >= '0') && (c <= '9');
}

}  // anonymous namespace

ToolchainVersion::ToolchainVersion(std::string text, std::vector<std::string> parts)
    : _text(std::move(text))
    , _parts(std::move(parts))
{
}

std::optional<ToolchainVersion> ToolchainVersion::parse(std::string_view text)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = text.find('.', start);
        const std::string_view part = text.substr(start, (dot == std::string_view::npos) ? std::string_view::npos : dot - start);
        if (part.empty() || (! std::ranges::all_of(part, isDigit))) {
            return std::nullopt;
        }
        parts.push_back(withoutLeadingZeros(part));
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return ToolchainVersion{ std::string{ text }, std::move(parts) };
}

const std::string& ToolchainVersion::text() const
{
    return _text;
}

bool ToolchainVersion::startsWith(const ToolchainVersion& prefix) const
{
    return (prefix._parts.size() <= _parts.size()) && std::equal(prefix._parts.begin(), prefix._parts.end(), _parts.begin());
}

std::strong_ordering ToolchainVersion::operator<=>(const ToolchainVersion& other) const
{
    const std::size_t shared = std::min(_parts.size(), other._parts.size());
    for (std::size_t i = 0; i < shared; ++i) {
        const std::strong_ordering order = compareParts(_parts[i], other._parts[i]);
        if (order != std::strong_ordering::equal) {
            return order;
        }
    }
    return _parts.size() <=> other._parts.size();
}

bool ToolchainVersion::operator==(const ToolchainVersion& other) const
{
    return _parts == other._parts;
}

}  // namespace scrap::Toolchain
