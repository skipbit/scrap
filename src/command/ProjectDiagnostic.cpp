#include "command/ProjectDiagnostic.h"

#include "project/ManifestError.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"

#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

namespace {

/// Next step for a path argument that cannot be searched from.
constexpr std::string_view PathHint =
    "hint: pass a directory inside a project, or omit the path to use the current directory\n";

auto render(const Project::NotADirectory& error) -> std::string
{
    std::string text = "error: '";
    text += error.path.string();
    text += "' is not a directory\n";
    text += PathHint;
    return text;
}

auto render(const Project::PathInaccessible& error) -> std::string
{
    std::string text = "error: cannot access '";
    text += error.path.string();
    text += "': ";
    text += error.reason;
    text += "\nhint: check the permissions of the path\n";
    return text;
}

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
 * A manifest that could not be read points at the file itself; one whose
 * contents are wrong points at editing it.
 */
auto render(const Project::ManifestError& error) -> std::string
{
    std::string text = Project::describe(error);
    if (error.kind == Project::ManifestErrorKind::Unreadable) {
        text += "\nhint: check that ";
        text += Project::ManifestFileName;
        text += " is a readable file\n";
    } else {
        text += "\nhint: correct ";
        text += Project::ManifestFileName;
        text += " and run the command again\n";
    }
    return text;
}

}  // anonymous namespace

auto renderProjectError(const Project::ProjectError& error) -> std::string
{
    return std::visit(
        [](const auto& alternative) -> std::string {
            return render(alternative);
        },
        error);
}

auto renderEmptyPathArgument() -> std::string
{
    std::string text = "error: the path argument is empty\n";
    text += PathHint;
    return text;
}

}  // namespace scrap::Command
