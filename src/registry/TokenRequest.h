#pragma once

#include "registry/BearerChallenge.h"

#include <optional>
#include <string>
#include <string_view>

namespace scrap::Registry {

/**
 * @brief The URL to ask for the token that @p challenge describes.
 *
 * The scope is the challenge's, or @p defaultScope when it names none.
 */
[[nodiscard]] std::string tokenUrl(const BearerChallenge& challenge, std::string_view defaultScope);

/**
 * @brief The token in the answer of a token endpoint, @p json.
 *
 * "token" is preferred to "access_token", and either must be a b64token of
 * RFC 6750, which keeps line breaks and spaces out of the header that carries
 * it.
 *
 * @return The token, or nothing when the answer is not JSON or holds none.
 */
[[nodiscard]] std::optional<std::string> readTokenAnswer(std::string_view json);

}  // namespace scrap::Registry
