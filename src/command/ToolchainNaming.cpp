#include "command/ToolchainNaming.h"

#include "command/ProjectDiagnostic.h"
#include "command/RuntimeEnvironment.h"
#include "command/ToolchainDiagnostic.h"
#include "project/ProjectLoader.h"
#include "toolchain/GlobalConfigFile.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainChoice.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

/**
 * @p text, named by @p source, understood as a choice. A name that is no
 * choice is reported as the place that gave it.
 */
std::expected<NamedToolchain, std::string> parseNamed(std::string text, const ChoiceSource source)
{
    auto choice = Toolchain::parseToolchainChoice(text);
    if (! choice.has_value()) {
        switch (source) {
        case ChoiceSource::Variable:
        case ChoiceSource::VariableOverProject:
            return std::unexpected(renderToolchainVariableNotASpecifier(text, choice.error()));
        case ChoiceSource::Project:
        case ChoiceSource::GlobalDefault:
            return std::unexpected(renderNotASpecifier(text, choice.error()));
        }
        std::unreachable();
    }
    return NamedToolchain{ .text = std::move(text), .choice = std::move(*choice), .source = source };
}

}  // anonymous namespace

std::string_view describeSource(const ChoiceSource source)
{
    switch (source) {
    case ChoiceSource::Variable:
        return "from SCRAP_TOOLCHAIN";
    case ChoiceSource::VariableOverProject:
        return "from SCRAP_TOOLCHAIN, overriding scrap.toml";
    case ChoiceSource::Project:
        return "from scrap.toml";
    case ChoiceSource::GlobalDefault:
        return "from the global default";
    }
    // Every source is answered above, so a source added without a word here
    // fails the build rather than being described as one of the others.
    std::unreachable();
}

std::string_view describeOrigin(const Toolchain::CompilerOrigin origin)
{
    switch (origin) {
    case Toolchain::CompilerOrigin::CompilerVariable:
        return "from CXX";
    case Toolchain::CompilerOrigin::DefaultOnPath:
        return "found on PATH";
    case Toolchain::CompilerOrigin::KnownName:
        return "found on PATH as a known name";
    }
    // Every origin is answered above, so an origin added without a word here
    // fails the build rather than being described as one of the others.
    std::unreachable();
}

std::string_view describeCOrigin(const Toolchain::CompilerOrigin origin)
{
    if (origin == Toolchain::CompilerOrigin::CompilerVariable) {
        return "from CC";
    }
    return describeOrigin(origin);
}

std::expected<std::optional<NamedToolchain>, std::string> namedAboveGlobalDefault(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain)
{
    if (! env.requestedToolchain.empty()) {
        return parseNamed(env.requestedToolchain, projectToolchain.has_value() ? ChoiceSource::VariableOverProject : ChoiceSource::Variable);
    }
    if (projectToolchain.has_value()) {
        return parseNamed(*projectToolchain, ChoiceSource::Project);
    }
    return std::nullopt;
}

std::expected<NamedAboveHere, std::string> namedAboveGlobalDefaultHere(const RuntimeEnvironment& env)
{
    const auto project = Project::loadProject(env.workingDirectory);
    if (! project.has_value() && ! std::holds_alternative<Project::ProjectNotFound>(project.error())) {
        return std::unexpected(renderProjectError(project.error()));
    }
    auto above = namedAboveGlobalDefault(env, project.has_value() ? project->manifest.toolchain : std::nullopt);
    if (! above.has_value()) {
        if (! project.has_value()) {
            return NamedAboveHere{};
        }
        return std::unexpected(std::move(above.error()));
    }
    return NamedAboveHere{ .named = std::move(*above), .inProject = project.has_value() };
}

std::expected<std::optional<NamedToolchain>, std::string> namedGlobalDefault(const RuntimeEnvironment& env)
{
    if (! env.scrapHome.has_value()) {
        return std::nullopt;
    }
    const std::filesystem::path file = Toolchain::globalConfigFile(*env.scrapHome);
    const auto read = Toolchain::readGlobalDefaultFile(file);
    if (! read.has_value()) {
        return std::unexpected(renderGlobalConfigFileError(file, read.error()));
    }
    const std::optional<std::string>& globalDefault = *read;
    if (! globalDefault.has_value()) {
        return std::nullopt;
    }
    return parseNamed(*globalDefault, ChoiceSource::GlobalDefault);
}

std::expected<std::optional<NamedToolchain>, std::string> namedToolchain(const RuntimeEnvironment& env, const std::optional<std::string>& projectToolchain)
{
    auto above = namedAboveGlobalDefault(env, projectToolchain);
    if ((! above.has_value()) || above->has_value()) {
        return above;
    }
    return namedGlobalDefault(env);
}

}  // namespace scrap::Command
