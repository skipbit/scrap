#include "registry/TokenRequest.h"

#include "json/JsonParser.h"
#include "json/JsonValue.h"
#include "registry/BearerChallenge.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Registry {

namespace {

constexpr std::array<char, 16> HexDigits{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
constexpr unsigned HighNibbleShift = 4;
constexpr unsigned NibbleMask = 0xF;

/**
 * @p text with everything but letters, digits and -._~ percent-encoded, for a
 * query parameter.
 */
std::string percentEncoded(const std::string_view text)
{
    std::string encoded;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if ((std::isalnum(byte) != 0) || (c == '-') || (c == '.') || (c == '_') || (c == '~')) {
            encoded.push_back(c);
        } else {
            encoded.push_back('%');
            encoded.push_back(HexDigits.at(byte >> HighNibbleShift));
            encoded.push_back(HexDigits.at(byte & NibbleMask));
        }
    }
    return encoded;
}

bool isTokenCharacter(const char c)
{
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || (c == '-') || (c == '.') || (c == '_') || (c == '~') || (c == '+') || (c == '/');
}

/**
 * Whether @p text may follow "Bearer " in a header line: the b64token of RFC
 * 6750, so that no line break or space reaches the request.
 */
bool isBearerToken(const std::string_view text)
{
    const std::size_t last = text.find_last_not_of('=');
    return (last != std::string_view::npos) && std::ranges::all_of(text.substr(0, last + 1), isTokenCharacter);
}

}  // anonymous namespace

std::string tokenUrl(const BearerChallenge& challenge, const std::string_view defaultScope)
{
    std::string url = challenge.realm;
    url += url.contains('?') ? '&' : '?';
    if (! challenge.service.empty()) {
        url += "service=" + percentEncoded(challenge.service) + "&";
    }
    url += "scope=" + percentEncoded(challenge.scope.empty() ? defaultScope : std::string_view{ challenge.scope });
    return url;
}

std::optional<std::string> readTokenAnswer(const std::string_view json)
{
    const auto answer = Json::parse(json);
    if (! answer.has_value()) {
        return std::nullopt;
    }
    for (const std::string_view name : { "token", "access_token" }) {
        const std::string* token = answer->stringMember(name);
        if ((token != nullptr) && isBearerToken(*token)) {
            return *token;
        }
    }
    return std::nullopt;
}

}  // namespace scrap::Registry
