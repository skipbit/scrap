#include "command/ProjectDiagnostic.h"

#include "project/ManifestError.h"
#include "project/ProjectCreator.h"
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

/// Next step for a path the operating system refused.
constexpr std::string_view PermissionHint = "hint: check the permissions of the path\n";

/// The rule a new project name follows, as scrap::Project::isValidProjectName() checks it.
constexpr std::string_view ProjectNameHint = "hint: use letters, digits, '-' and '_', starting with a letter\n";

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
    text += '\n';
    text += PermissionHint;
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

/**
 * Text the user typed, made safe to print: printable ASCII stays as it is and
 * every other byte becomes \xNN, so control characters and escape sequences
 * reach the terminal as text rather than as instructions.
 */
auto printable(std::string_view text) -> std::string
{
    static constexpr std::string_view HexDigits = "0123456789ABCDEF";
    std::string result;
    for (const char ch : text) {
        const unsigned byte = static_cast<unsigned char>(ch);
        if (byte >= 0x20U && byte < 0x7FU) {
            result += ch;
        } else {
            result += "\\x";
            result += HexDigits[byte >> 4U];
            result += HexDigits[byte & 0x0FU];
        }
    }
    return result;
}

auto render(const Project::InvalidProjectName& error) -> std::string
{
    std::string text;
    if (error.name.empty()) {
        text = "error: the project name is empty\n";
    } else {
        text = "error: '";
        text += printable(error.name);
        text += "' is not a valid project name\n";
    }
    text += ProjectNameHint;
    return text;
}

auto render(const Project::PathExists& error) -> std::string
{
    std::string text = "error: '";
    text += error.path.string();
    text += "' already exists\n";
    text += "hint: choose another name, or run the command in another directory\n";
    return text;
}

auto render(const Project::CannotCreate& error) -> std::string
{
    std::string text = "error: cannot create '";
    text += error.path.string();
    text += "': ";
    text += error.reason;
    text += '\n';
    text += PermissionHint;
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

auto renderCreateProjectError(const Project::CreateProjectError& error) -> std::string
{
    return std::visit(
        [](const auto& alternative) -> std::string {
            return render(alternative);
        },
        error);
}

}  // namespace scrap::Command
