#include "digest/DigestText.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace scrap::Digest {

namespace {

constexpr std::size_t Sha256HexLength = 64;

bool isLowercaseHex(const char c)
{
    return ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f'));
}

}  // anonymous namespace

bool isSha256Hex(const std::string_view text)
{
    return (text.size() == Sha256HexLength) && std::ranges::all_of(text, isLowercaseHex);
}

bool isSha256Digest(const std::string_view text)
{
    return text.starts_with(Sha256Prefix) && isSha256Hex(text.substr(Sha256Prefix.size()));
}

std::string sha256Digest(const std::string_view hex)
{
    return std::string{ Sha256Prefix }.append(hex);
}

}  // namespace scrap::Digest
