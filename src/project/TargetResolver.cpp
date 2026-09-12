#include "project/TargetResolver.h"

#include "project/Manifest.h"

#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Project {

namespace {

/// Entry point assumed when the manifest declares no targets.
constexpr std::string_view DefaultEntryPoint = "src/main.cpp";

}  // anonymous namespace

/**
 * Return the manifest's targets, or the one inferred from the default layout.
 */
auto resolveTargets(const std::filesystem::path& projectRoot, const Manifest& manifest) -> std::vector<Target>
{
    if (manifest.declaresTargets) {
        return manifest.targets;
    }

    const std::filesystem::path entryPoint{DefaultEntryPoint};
    std::error_code ec;
    if (! std::filesystem::is_regular_file(projectRoot / entryPoint, ec)) {
        return {};
    }

    return {Target{.kind = TargetKind::Executable, .name = manifest.package.name, .entryPoint = entryPoint}};
}

}  // namespace scrap::Project
