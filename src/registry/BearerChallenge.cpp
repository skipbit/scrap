#include "registry/BearerChallenge.h"

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Registry {

namespace {

constexpr std::string_view Scheme = "bearer";

bool isSpace(const char c)
{
    return (c == ' ') || (c == '\t');
}

void skipSpaces(std::string_view& text)
{
    while ((! text.empty()) && isSpace(text.front())) {
        text.remove_prefix(1);
    }
}

bool isTokenCharacter(const char c)
{
    return (std::isalnum(static_cast<unsigned char>(c)) != 0) || std::string_view{ "!#$%&'*+-.^_`|~" }.contains(c);
}

std::string_view takeToken(std::string_view& text)
{
    std::size_t length = 0;
    while ((length < text.size()) && isTokenCharacter(text[length])) {
        ++length;
    }
    const std::string_view token = text.substr(0, length);
    text.remove_prefix(length);
    return token;
}

/**
 * A quoted string at the start of @p text, with its escapes resolved.
 */
std::optional<std::string> takeQuoted(std::string_view& text)
{
    text.remove_prefix(1);  // the opening quote
    std::string value;
    while (! text.empty()) {
        const char c = text.front();
        text.remove_prefix(1);
        if (c == '"') {
            return value;
        }
        if (c == '\\') {
            if (text.empty()) {
                return std::nullopt;
            }
            value.push_back(text.front());
            text.remove_prefix(1);
            continue;
        }
        value.push_back(c);
    }
    return std::nullopt;
}

bool equalsIgnoringCase(const std::string_view a, const std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/**
 * Read one name=value parameter from the start of @p text into @p challenge.
 */
bool takeParameter(std::string_view& text, BearerChallenge& challenge)
{
    const std::string_view name = takeToken(text);
    skipSpaces(text);
    if (name.empty() || text.empty() || (text.front() != '=')) {
        return false;
    }
    text.remove_prefix(1);
    skipSpaces(text);
    std::string value;
    if ((! text.empty()) && (text.front() == '"')) {
        auto quoted = takeQuoted(text);
        if (! quoted.has_value()) {
            return false;
        }
        value = std::move(*quoted);
    } else {
        value = std::string{ takeToken(text) };
    }

    if (equalsIgnoringCase(name, "realm")) {
        challenge.realm = std::move(value);
    } else if (equalsIgnoringCase(name, "service")) {
        challenge.service = std::move(value);
    } else if (equalsIgnoringCase(name, "scope")) {
        challenge.scope = std::move(value);
    }
    return true;
}

}  // anonymous namespace

std::optional<BearerChallenge> readBearerChallenge(std::string_view header)
{
    skipSpaces(header);
    if (! equalsIgnoringCase(takeToken(header), Scheme)) {
        return std::nullopt;
    }

    BearerChallenge challenge;
    while (true) {
        skipSpaces(header);
        if (header.empty()) {
            break;
        }
        if (! takeParameter(header, challenge)) {
            return std::nullopt;
        }
        skipSpaces(header);
        if (header.empty()) {
            break;
        }
        if (header.front() != ',') {
            return std::nullopt;
        }
        header.remove_prefix(1);
    }
    if (challenge.realm.empty()) {
        return std::nullopt;
    }
    return challenge;
}

}  // namespace scrap::Registry
