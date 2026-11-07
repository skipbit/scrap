#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace scrap::Registry {

/**
 * @brief Where a registry hands out the token it asks for, and for what.
 */
struct BearerChallenge {
    std::string realm;    ///< The URL that hands out tokens.
    std::string service;  ///< Empty when the challenge names none.
    std::string scope;    ///< Empty when the challenge names none.
};

/**
 * @brief Read the WWW-Authenticate header of a registry that asks for a bearer
 *        token, such as: Bearer realm="https://ghcr.io/token",service="ghcr.io".
 *
 * Parameters other than realm, service and scope are passed over.
 *
 * @return The challenge, or nothing when the header asks for something other
 *         than a bearer token, names no realm, or cannot be read.
 */
[[nodiscard]] std::optional<BearerChallenge> readBearerChallenge(std::string_view header);

}  // namespace scrap::Registry
