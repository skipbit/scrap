#include "compile/CompilePlanner.h"

#include "compile/CompileCommand.h"
#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Compile {

namespace {

/// Directory below the build directory that holds object files.
constexpr std::string_view ObjectDirectory = "obj";

/// Directory the default layout keeps public headers in.
constexpr std::string_view HeaderDirectory = "include";

}  // anonymous namespace

auto planCompileCommands(const std::filesystem::path& projectRoot,
                         const Project::Package& package,
                         const std::vector<Project::TargetSources>& targets,
                         const std::filesystem::path& compiler) -> std::vector<CompileCommand>
{
    const std::filesystem::path objectRoot = std::filesystem::path{DebugBuildDirectory} / ObjectDirectory;

    std::vector<CompileCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        for (const std::filesystem::path& source : entry.sources) {
            std::filesystem::path output = objectRoot / entry.target.name / source;
            output += ".o";
            std::vector<std::string> arguments{compiler.string(),
                                               "-std=c++" + package.standard,
                                               "-I",
                                               std::string{HeaderDirectory},
                                               "-c",
                                               source.string(),
                                               "-o",
                                               output.string()};
            commands.push_back(CompileCommand{
                .directory = projectRoot, .file = source, .output = output, .arguments = std::move(arguments)});
        }
    }
    return commands;
}

}  // namespace scrap::Compile
