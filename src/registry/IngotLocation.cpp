#include "registry/IngotLocation.h"

#include "fetch/UrlPolicy.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Registry {

namespace {

constexpr std::size_t MaxTagLength = 128;
constexpr std::string_view LowercaseAlphanumerics = "abcdefghijklmnopqrstuvwxyz0123456789";

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

/**
 * Whether @p run, found between two letters or digits of a path component,
 * is a separator OCI allows there: a period, one or two underscores, or
 * dashes.
 */
bool isSeparator(const std::string_view run)
{
    return (run == ".") || (run == "_") || (run == "__") || (run.find_first_not_of('-') == std::string_view::npos);
}

/**
 * Whether @p component matches [a-z0-9]+((\.|_|__|-+)[a-z0-9]+)*, a path
 * component of an OCI repository name.
 */
bool isPathComponent(const std::string_view component)
{
    if (component.empty() || (component.find_first_of(LowercaseAlphanumerics) != 0) || (component.find_last_of(LowercaseAlphanumerics) != component.size() - 1)) {
        return false;
    }
    std::size_t separator = component.find_first_not_of(LowercaseAlphanumerics);
    while (separator != std::string_view::npos) {
        const std::size_t next = component.find_first_of(LowercaseAlphanumerics, separator);
        if (! isSeparator(component.substr(separator, next - separator))) {
            return false;
        }
        separator = component.find_first_not_of(LowercaseAlphanumerics, next);
    }
    return true;
}

/**
 * Whether @p repository is a name an OCI registry takes: path components
 * joined by slashes.
 */
bool isRepositoryName(const std::string_view repository)
{
    std::size_t start = 0;
    while (true) {
        const std::size_t slash = repository.find('/', start);
        if (! isPathComponent(repository.substr(start, slash - start))) {
            return false;
        }
        if (slash == std::string_view::npos) {
            return true;
        }
        start = slash + 1;
    }
}

/**
 * Whether @p reference matches [A-Za-z0-9_][A-Za-z0-9._-]{0,127}, an OCI tag.
 */
bool isTagReference(const std::string_view reference)
{
    const auto isWordCharacter = [](const char c) {
        return (std::isalnum(static_cast<unsigned char>(c)) != 0) || (c == '_');
    };
    return (! reference.empty()) && (reference.size() <= MaxTagLength) && isWordCharacter(reference.front()) && std::ranges::all_of(reference, [&isWordCharacter](const char c) {
        return isWordCharacter(c) || (c == '.') || (c == '-');
    });
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
    if (authority.empty() || hostOf(authority).empty() || (colon == std::string_view::npos) || (! isTagReference(tag.substr(colon + 1)))) {
        return std::nullopt;
    }
    const std::string_view name = tag.substr(0, colon);
    std::string repository = path.empty() ? std::string{ name } : std::string{ path } + "/" + std::string{ name };
    if (! isRepositoryName(repository)) {
        return std::nullopt;
    }

    const std::string scheme = Fetch::isThisMachine(hostOf(authority)) ? "http" : "https";
    return IngotLocation{ .origin = scheme + "://" + std::string{ authority }, .repository = std::move(repository), .digest = std::string{ digest } };
}

}  // namespace scrap::Registry
