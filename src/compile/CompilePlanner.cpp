#include "compile/CompilePlanner.h"

#include "compile/ArchiveCommand.h"
#include "compile/BuildProfile.h"
#include "compile/CompileCommand.h"
#include "compile/CompilerDriver.h"
#include "compile/LinkCommand.h"
#include "project/Manifest.h"
#include "project/SourceCollector.h"

#include <array>
#include <filesystem>
#include <optional>
#include <span>
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

/// Directory below the build directory that holds static libraries.
constexpr std::string_view LibraryDirectory = "lib";

/// Directory the default layout keeps public headers in.
constexpr std::string_view HeaderDirectory = "include";

/// What a debug build asks of the compiler: debugging information, no
/// optimisation, and the common warnings.
constexpr std::array<std::string_view, 5> DebugOptions{ "-g", "-O0", "-Wall", "-Wextra", "-Wpedantic" };

/// What a release build asks of the compiler: optimisation, no assertions,
/// and the same warnings, some of which only optimisation brings out.
constexpr std::array<std::string_view, 5> ReleaseOptions{ "-O3", "-DNDEBUG", "-Wall", "-Wextra", "-Wpedantic" };

/**
 * The options that build for @p profile.
 */
std::span<const std::string_view> profileOptions(const BuildProfile profile)
{
    switch (profile) {
    case BuildProfile::Debug:
        return DebugOptions;
    case BuildProfile::Release:
        return ReleaseOptions;
    }
    std::unreachable();
}

/**
 * @p path as it goes on the command line: a relative path that starts with
 * '-' or '@' gains a leading ./, so the compiler reads it as a file. An
 * argument starting with '@' names a file of options the compiler reads
 * before anything else, which would let a source name decide the command.
 */
std::filesystem::path asArgument(const std::filesystem::path& path)
{
    if (path.is_relative() && (path.native().starts_with('-') || path.native().starts_with('@'))) {
        return std::filesystem::path{ "." } / path;
    }
    return path;
}

/**
 * Where @p source is compiled to for @p target.
 */
std::filesystem::path objectFile(const BuildSettings& settings, const std::string& target, const std::filesystem::path& source)
{
    std::filesystem::path output = settings.buildDirectory / ObjectDirectory / target / source;
    output += ".o";
    return output;
}

/**
 * The command line every compilation shares, up to the source it compiles.
 */
std::vector<std::string> sharedCompileArguments(const BuildSettings& settings)
{
    std::vector<std::string> arguments{
        settings.compiler.string(),
        settings.driver.standardOption(settings.standard).value_or(standardNameOption(settings.standard))
    };
    const std::span<const std::string_view> options = profileOptions(settings.profile);
    arguments.insert(arguments.end(), options.begin(), options.end());
    if (const auto color = settings.driver.colorOption(); color.has_value()) {
        arguments.push_back(*color);
    }
    arguments.insert(arguments.end(), { "-I", std::string{ HeaderDirectory } });
    return arguments;
}

/**
 * The settings that reach the commands of @p target, in the order they go
 * on the command line: its own, its public ones, then the public ones of
 * each library among @p targets that it uses.
 */
std::vector<const Project::TargetSettings*> appliedSettings(const Project::Target& target, const std::vector<Project::TargetSources>& targets)
{
    std::vector<const Project::TargetSettings*> applied{ &target.settings, &target.publicSettings };
    if (target.kind == Project::TargetKind::Executable) {
        for (const Project::TargetSources& entry : targets) {
            if (entry.target.kind == Project::TargetKind::Library) {
                applied.push_back(&entry.target.publicSettings);
            }
        }
    }
    return applied;
}

/**
 * What @p applied adds to a compilation: include directories, then defines,
 * then compile flags, each kind in the order of @p applied.
 */
std::vector<std::string> settingsCompileArguments(const std::vector<const Project::TargetSettings*>& applied)
{
    std::vector<std::string> arguments;
    for (const Project::TargetSettings* settings : applied) {
        for (const std::filesystem::path& directory : settings->includeDirectories) {
            arguments.insert(arguments.end(), { "-I", asArgument(directory).string() });
        }
    }
    for (const Project::TargetSettings* settings : applied) {
        for (const std::string& define : settings->defines) {
            arguments.push_back("-D" + define);
        }
    }
    for (const Project::TargetSettings* settings : applied) {
        arguments.insert(arguments.end(), settings->compileFlags.begin(), settings->compileFlags.end());
    }
    return arguments;
}

/**
 * What @p applied adds to a link, in the order of @p applied.
 */
std::vector<std::string> settingsLinkArguments(const std::vector<const Project::TargetSettings*>& applied)
{
    std::vector<std::string> arguments;
    for (const Project::TargetSettings* settings : applied) {
        arguments.insert(arguments.end(), settings->linkFlags.begin(), settings->linkFlags.end());
    }
    return arguments;
}

/**
 * The object files planCompileCommands() gives the sources of @p entry, in
 * the same order.
 */
std::vector<std::string> objectFiles(const BuildSettings& settings, const Project::TargetSources& entry)
{
    std::vector<std::string> objects;
    objects.reserve(entry.sources.size());
    for (const std::filesystem::path& source : entry.sources) {
        objects.push_back(objectFile(settings, entry.target.name, source).string());
    }
    return objects;
}

}  // anonymous namespace

std::vector<CompileCommand> planCompileCommands(const BuildSettings& settings, const std::vector<Project::TargetSources>& targets)
{
    const std::vector<std::string> shared = sharedCompileArguments(settings);

    std::vector<CompileCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        const std::vector<std::string> added = settingsCompileArguments(appliedSettings(entry.target, targets));
        for (const std::filesystem::path& source : entry.sources) {
            const std::filesystem::path output = objectFile(settings, entry.target.name, source);
            const std::filesystem::path file = asArgument(source);

            std::vector<std::string> arguments = shared;
            arguments.insert(arguments.end(), added.begin(), added.end());
            arguments.insert(arguments.end(), { "-c", file.string(), "-o", output.string() });
            commands.push_back(CompileCommand{ .target = entry.target.name, .directory = settings.projectRoot, .file = file, .output = output, .arguments = std::move(arguments) });
        }
    }
    return commands;
}

std::filesystem::path executableFile(const std::filesystem::path& buildDirectory, std::string_view target)
{
    return buildDirectory / ExecutableDirectory / target;
}

std::filesystem::path libraryFile(const std::filesystem::path& buildDirectory, std::string_view target)
{
    std::filesystem::path file = buildDirectory / LibraryDirectory;
    file /= "lib" + std::string{ target } + ".a";
    return file;
}

std::vector<ArchiveCommand> planArchiveCommands(const BuildSettings& settings,
                                                const std::filesystem::path& archiver,
                                                const std::vector<Project::TargetSources>& targets)
{
    std::vector<ArchiveCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        if (entry.target.kind != Project::TargetKind::Library) {
            continue;
        }
        const std::filesystem::path output = libraryFile(settings.buildDirectory, entry.target.name);

        std::vector<std::string> arguments{ archiver.string(), "rcs", output.string() };
        const std::vector<std::string> objects = objectFiles(settings, entry);
        arguments.insert(arguments.end(), objects.begin(), objects.end());
        commands.push_back(ArchiveCommand{ .target = entry.target.name, .directory = settings.projectRoot, .output = output, .arguments = std::move(arguments) });
    }
    return commands;
}

std::vector<LinkCommand> planLinkCommands(const BuildSettings& settings, const std::vector<Project::TargetSources>& targets)
{
    std::vector<std::string> libraries;
    for (const Project::TargetSources& entry : targets) {
        if (entry.target.kind == Project::TargetKind::Library) {
            libraries.push_back(libraryFile(settings.buildDirectory, entry.target.name).string());
        }
    }

    std::vector<LinkCommand> commands;
    for (const Project::TargetSources& entry : targets) {
        if (entry.target.kind != Project::TargetKind::Executable) {
            continue;
        }
        const std::filesystem::path output = executableFile(settings.buildDirectory, entry.target.name);

        std::vector<std::string> arguments{ settings.compiler.string() };
        if (const auto color = settings.driver.colorOption(); color.has_value()) {
            arguments.push_back(*color);
        }
        const std::vector<std::string> objects = objectFiles(settings, entry);
        arguments.insert(arguments.end(), objects.begin(), objects.end());
        arguments.insert(arguments.end(), libraries.begin(), libraries.end());
        const std::vector<std::string> added = settingsLinkArguments(appliedSettings(entry.target, targets));
        arguments.insert(arguments.end(), added.begin(), added.end());
        arguments.insert(arguments.end(), { "-o", output.string() });
        commands.push_back(LinkCommand{ .target = entry.target.name, .directory = settings.projectRoot, .output = output, .arguments = std::move(arguments) });
    }
    return commands;
}

}  // namespace scrap::Compile
