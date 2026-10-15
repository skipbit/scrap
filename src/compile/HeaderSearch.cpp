#include "compile/HeaderSearch.h"

#include "compile/IncludeDirectives.h"

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

namespace scrap::Compile {

HeaderSearch headerSearch(const IncludedName& included, const std::filesystem::path& directory, const std::vector<std::filesystem::path>& searched)
{
    HeaderSearch search{ .paths = {}, .stopsAtAFile = included.kind != IncludeKind::Next };
    const auto add = [&search, &included](const std::filesystem::path& place) {
        std::filesystem::path path = (place / included.name).lexically_normal();
        if (std::ranges::find(search.paths, path) == search.paths.end()) {
            search.paths.push_back(std::move(path));
        }
    };
    if (included.kind != IncludeKind::Angled) {
        add(directory);
    }
    for (const std::filesystem::path& place : searched) {
        add(place);
    }
    return search;
}

}  // namespace scrap::Compile
