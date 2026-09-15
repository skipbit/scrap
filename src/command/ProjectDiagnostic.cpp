#include "command/ProjectDiagnostic.h"

#include "project/ManifestError.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"

#include <string>
#include <variant>

namespace scrap::Command {

namespace {

/**
 * Render a start path that is not a directory.
 */
auto render(const Project::NotADirectory& error) -> std::string
{
    std::string text = "error: '";
    text += error.path.string();
    text += "' is not a directory\n";
    text += "hint: pass a directory inside a project, or omit the path to use the current directory\n";
    return text;
}

/**
 * Render a search that found no manifest.
 */
auto render(const Project::ProjectNotFound& error) -> std::string
{
    std::string text = "error: could not find ";
    text += Project::ManifestFileName;
    text += " in '";
    text += error.startDir.string();
    text += "' or any parent directory\n";
    text += "hint: run 'scrap new <name>' to create a project\n";
    return text;
}

/**
 * Render a manifest that could not be read or parsed.
 */
auto render(const Project::ManifestError& error) -> std::string
{
    std::string text = Project::describe(error);
    text += "\nhint: correct ";
    text += Project::ManifestFileName;
    text += " and run the command again\n";
    return text;
}

}  // anonymous namespace

/**
 * Render whichever error the project loader returned.
 */
auto renderProjectError(const Project::ProjectError& error) -> std::string
{
    return std::visit(
        [](const auto& alternative) -> std::string {
            return render(alternative);
        },
        error);
}

}  // namespace scrap::Command
