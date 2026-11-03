#include "tar/TarPath.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace scrap::Tar {

namespace {

constexpr std::string_view ParentDirectory = "..";

/// The components of @p path, without "." and empty ones.
std::vector<std::string_view> splitPath(std::string_view path)
{
    std::vector<std::string_view> components;
    while (! path.empty()) {
        const std::size_t slash = path.find('/');
        const std::string_view component = path.substr(0, slash);
        if (! component.empty() && component != ".") {
            components.push_back(component);
        }
        path.remove_prefix(slash == std::string_view::npos ? path.size() : slash + 1);
    }
    return components;
}

}  // anonymous namespace

std::optional<std::vector<std::string_view>> memberComponents(std::string_view path)
{
    if (path.empty() || path.front() == '/') {
        return std::nullopt;
    }
    auto components = splitPath(path);
    for (const std::string_view component : components) {
        if (component == ParentDirectory) {
            return std::nullopt;
        }
    }
    return components;
}

bool symbolicLinkStaysInside(std::span<const std::string_view> link, std::string_view target)
{
    if (link.empty() || target.empty() || target.front() == '/') {
        return false;
    }

    const auto components = splitPath(target);
    std::size_t levelsUp = 0;
    while (levelsUp < components.size() && components[levelsUp] == ParentDirectory) {
        ++levelsUp;
    }
    for (std::size_t later = levelsUp; later < components.size(); ++later) {
        if (components[later] == ParentDirectory) {
            return false;
        }
    }
    return levelsUp < link.size();
}

}  // namespace scrap::Tar
