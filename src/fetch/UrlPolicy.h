#pragma once

#include <string>
#include <string_view>

namespace scrap::Fetch {

/**
 * @brief What a URL says about where it leads.
 */
struct UrlTarget {
    std::string scheme;
    std::string host;
};

/**
 * @brief Whether @p host names the machine the process runs on.
 */
[[nodiscard]] bool isThisMachine(std::string_view host);

/**
 * @brief Whether @p target may be fetched: https always, http only to this
 *        machine.
 */
[[nodiscard]] bool isAccepted(const UrlTarget& target);

/**
 * @brief Whether fetching @p target needs root certificates to verify the host.
 */
[[nodiscard]] bool needsCertificates(const UrlTarget& target);

}  // namespace scrap::Fetch
