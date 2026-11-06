#include "registry/IngotLocation.h"

#include "fetch/UrlPolicy.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Registry {

namespace {

/**
 * The host of @p authority without its port: [::1] stays bracketed.
 */
std::string_view hostOf(const std::string_view authority)
{
    if (authority.starts_with('[')) {
        const std::size_t close = authority.find(']');
        return (close == std::string_view::npos) ? authority : authority.substr(0, close + 1);
    }
    return authority.substr(0, authority.find(':'));
}

}  // anonymous namespace

std::string IngotLocation::manifestUrl() const
{
    return origin + "/v2/" + repository + "/manifests/" + digest;
}

std::string IngotLocation::blobUrl(const std::string_view blobDigest) const
{
    return origin + "/v2/" + repository + "/blobs/" + std::string{ blobDigest };
}

std::string IngotLocation::pullScope() const
{
    return "repository:" + repository + ":pull";
}

std::optional<IngotLocation> locateIngot(const std::string_view registry, const std::string_view tag, const std::string_view digest)
{
    const std::size_t slash = registry.find('/');
    const std::string_view authority = registry.substr(0, slash);
    const std::string_view path = (slash == std::string_view::npos) ? std::string_view{} : registry.substr(slash + 1);
    const std::size_t colon = tag.find(':');
    const std::string_view name = tag.substr(0, colon);
    if (authority.empty() || hostOf(authority).empty() || (colon == std::string_view::npos) || name.empty()) {
        return std::nullopt;
    }

    const std::string scheme = Fetch::isThisMachine(hostOf(authority)) ? "http" : "https";
    std::string repository = path.empty() ? std::string{ name } : std::string{ path } + "/" + std::string{ name };
    return IngotLocation{ .origin = scheme + "://" + std::string{ authority }, .repository = std::move(repository), .digest = std::string{ digest } };
}

}  // namespace scrap::Registry
