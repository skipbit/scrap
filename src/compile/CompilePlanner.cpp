#include "compile/CompilePlanner.h"

#include "compile/CompileCommand.h"
#include "project/LanguageStandard.h"
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

/**
 * @p path as it goes on the command line: a relative path that starts with
 * '-' gains a leading ./, so the compiler reads it as a file.
 */
auto asArgument(const std::filesystem::path& path) -> std::filesystem::path
{
    if (path.is_relative() && path.native().starts_with('-')) {
        return std::filesystem::path{"."} / path;
    }
    return path;
}

}  // anonymous namespace

auto planCompileCommands(const std::filesystem::path& projectRoot,
                         const std::filesystem::path& buildDirectory,
                         const Project::Package& package,
                         const std::vector<Project::TargetSources>& targets,
                         const std::filesystem::path& compiler) -> std::vector<CompileCommand>
{
    const std::filesystem::path objectRoot = buildDirectory / ObjectDirectory;
    const std::vector<std::string> shared{compiler.string(),
                                          "-std=c++" + std::string{Project::standardNumber(package.standard)},
                                          "-I",
                                          std::string{HeaderDirectory}};

    std::vector<CompileCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        for (const std::filesystem::path& source : entry.sources) {
            std::filesystem::path output = objectRoot / entry.target.name / source;
            output += ".o";
            const std::filesystem::path file = asArgument(source);

            std::vector<std::string> arguments = shared;
            arguments.insert(arguments.end(), {"-c", file.string(), "-o", output.string()});
            commands.push_back(CompileCommand{
                .directory = projectRoot, .file = file, .output = output, .arguments = std::move(arguments)});
        }
    }
    return commands;
}

}  // namespace scrap::Compile
