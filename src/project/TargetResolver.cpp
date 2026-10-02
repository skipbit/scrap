#include "project/TargetResolver.h"

#include "project/Manifest.h"

#include <algorithm>
#include <filesystem>
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

    return { Target{ .kind = TargetKind::Executable, .name = manifest.package.name, .entryPoint = entryPoint } };
}

/**
 * Return the target of that name, or nothing when there is none.
 */
std::optional<std::vector<Target>> targetsToBuild(const std::vector<Target>& targets, const std::string_view name)
{
    const auto found = std::ranges::find(targets, name, &Target::name);
    if (found == targets.end()) {
        return std::nullopt;
    }
    return std::vector<Target>{ *found };
}

}  // namespace scrap::Project
