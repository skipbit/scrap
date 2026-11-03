#include "fetch/UrlPolicy.h"

#include <string_view>

namespace scrap::Fetch {

bool isThisMachine(const std::string_view host)
{
    return (host == "127.0.0.1") || (host == "[::1]") || (host == "localhost");
}

bool isAccepted(const UrlTarget& target)
{
    return (target.scheme == "https") || ((target.scheme == "http") && isThisMachine(target.host));
}

bool needsCertificates(const UrlTarget& target)
{
    return target.scheme == "https";
}

}  // namespace scrap::Fetch
