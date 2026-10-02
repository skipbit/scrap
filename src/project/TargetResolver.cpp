#include "project/TargetResolver.h"

#include "project/Manifest.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Project {

/**
 * Return the manifest's targets, or the one inferred from the default layout.
 */
std::vector<Target> resolveTargets(const std::filesystem::path& projectRoot, const Manifest& manifest)
{
    if (manifest.declaresTargets) {
        return manifest.targets;
    }

    const std::filesystem::path entryPoint{ DefaultEntryPoint };
    std::error_code ec;
    if (! std::filesystem::is_regular_file(projectRoot / entryPoint, ec)) {
        return {};
    }

    return { Target{ .kind = TargetKind::Executable, .name = manifest.package.name, .source = entryPoint } };
}

/**
 * Return the target of that name with the library it uses, or nothing when
 * there is no target of that name.
 */
std::optional<std::vector<Target>> targetsToBuild(const std::vector<Target>& targets, const std::string_view name)
{
    const auto found = std::ranges::find(targets, name, &Target::name);
    if (found == targets.end()) {
        return std::nullopt;
    }
    std::vector<Target> needed{ *found };
    if (found->kind == TargetKind::Executable) {
        std::ranges::copy_if(targets, std::back_inserter(needed), [](const Target& target) {
            return target.kind == TargetKind::Library;
        });
    }
    return needed;
}

}  // namespace scrap::Project
