#include "compile/CompilePlanner.h"

#include "compile/CompileCommand.h"
#include "compile/CompilerDriver.h"
#include "compile/LinkCommand.h"
#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Compile {

namespace {

/// Directory below the build directory that holds object files.
constexpr std::string_view ObjectDirectory = "obj";

/// Directory below the build directory that holds executables.
constexpr std::string_view ExecutableDirectory = "bin";

/// Directory the default layout keeps public headers in.
constexpr std::string_view HeaderDirectory = "include";

/// What a debug build asks of the compiler: debugging information, no
/// optimisation, and the common warnings.
constexpr std::array<std::string_view, 5> DebugOptions{"-g", "-O0", "-Wall", "-Wextra", "-Wpedantic"};

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

/**
 * Where @p source is compiled to for @p target.
 */
auto objectFile(const BuildSettings& settings,
                const std::string& target,
                const std::filesystem::path& source) -> std::filesystem::path
{
    std::filesystem::path output = settings.buildDirectory / ObjectDirectory / target / source;
    output += ".o";
    return output;
}

/**
 * The command line every compilation shares, up to the source it compiles.
 */
auto sharedCompileArguments(const BuildSettings& settings) -> std::vector<std::string>
{
    std::vector<std::string> arguments{
        settings.compiler.string(),
        settings.driver.standardOption(settings.standard).value_or(standardNameOption(settings.standard))};
    arguments.insert(arguments.end(), DebugOptions.begin(), DebugOptions.end());
    if (const auto color = settings.driver.colorOption(); color.has_value()) {
        arguments.push_back(*color);
    }
    arguments.insert(arguments.end(), {"-I", std::string{HeaderDirectory}});
    return arguments;
}

}  // anonymous namespace

auto planCompileCommands(const BuildSettings& settings,
                         const std::vector<Project::TargetSources>& targets) -> std::vector<CompileCommand>
{
    const std::vector<std::string> shared = sharedCompileArguments(settings);

    std::vector<CompileCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        for (const std::filesystem::path& source : entry.sources) {
            const std::filesystem::path output = objectFile(settings, entry.target.name, source);
            const std::filesystem::path file = asArgument(source);

            std::vector<std::string> arguments = shared;
            arguments.insert(arguments.end(), {"-c", file.string(), "-o", output.string()});
            commands.push_back(CompileCommand{.target = entry.target.name,
                                              .directory = settings.projectRoot,
                                              .file = file,
                                              .output = output,
                                              .arguments = std::move(arguments)});
        }
    }
    return commands;
}

auto planLinkCommands(const BuildSettings& settings,
                      const std::vector<Project::TargetSources>& targets) -> std::vector<LinkCommand>
{
    std::vector<LinkCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        if (entry.target.kind != Project::TargetKind::Executable) {
            continue;
        }
        const std::filesystem::path output = settings.buildDirectory / ExecutableDirectory / entry.target.name;

        std::vector<std::string> arguments{settings.compiler.string()};
        if (const auto color = settings.driver.colorOption(); color.has_value()) {
            arguments.push_back(*color);
        }
        for (const std::filesystem::path& source : entry.sources) {
            arguments.push_back(objectFile(settings, entry.target.name, source).string());
        }
        arguments.insert(arguments.end(), {"-o", output.string()});
        commands.push_back(LinkCommand{.target = entry.target.name,
                                       .directory = settings.projectRoot,
                                       .output = output,
                                       .arguments = std::move(arguments)});
    }
    return commands;
}

}  // namespace scrap::Compile
