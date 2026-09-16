#include "command/ProjectDiagnostic.h"

#include "project/ManifestError.h"
#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace scrap::Command {

namespace {

/// Next step for a path argument that cannot be searched from.
constexpr std::string_view PathHint =
    "hint: pass a directory inside a project, or omit the path to use the current directory\n";

/// Next step for a path the operating system refused.
constexpr std::string_view PermissionHint = "hint: check the permissions of the path\n";

/**
 * The rule a new project name follows, as scrap::Project::isValidProjectName()
 * checks it.
 */
auto projectNameHint() -> std::string
{
    std::string text = "hint: use up to ";
    text += std::to_string(Project::MaxProjectNameLength);
    text += " letters, digits, '-' and '_', starting with a letter\n";
    return text;
}

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
    text += "hint: run 'scrap new <project-name>' to create a project\n";
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
 * Append @p byte to @p text as \xNN.
 */
auto appendEscaped(const unsigned byte, std::string& text) -> void
{
    static constexpr std::string_view HexDigits = "0123456789ABCDEF";
    text += "\\x";
    text += HexDigits[byte >> 4U];
    text += HexDigits[byte & 0x0FU];
}

/**
 * Text the user typed, made safe to print: printable ASCII stays as it is and
 * every other byte becomes \xNN, so control characters and escape sequences
 * reach the terminal as text rather than as instructions. A backslash is
 * escaped as well, which leaves \xNN as the mark of an escaped byte alone.
 */
auto printable(std::string_view text) -> std::string
{
    std::string result;
    for (const char ch : text) {
        const unsigned byte = static_cast<unsigned char>(ch);
        if (byte >= 0x20U && byte < 0x7FU && ch != '\\') {
            result += ch;
        } else {
            appendEscaped(byte, result);
        }
    }
    return result;
}

/**
 * A path made safe to print. A path carries the working directory, whose name
 * can hold any byte, so control characters and a backslash are escaped while
 * letters outside ASCII stay as they are and keep the path readable. The two
 * bytes of a C1 control character are escaped together, since a terminal acts
 * on them as one.
 */
auto printablePath(const std::filesystem::path& path) -> std::string
{
    const std::string text = path.string();
    std::string result;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const unsigned byte = static_cast<unsigned char>(text[index]);
        const unsigned next = index + 1 < text.size() ? static_cast<unsigned char>(text[index + 1]) : 0U;
        if (byte == 0xC2U && next >= 0x80U && next <= 0x9FU) {
            appendEscaped(byte, result);
            appendEscaped(next, result);
            ++index;
        } else if (byte < 0x20U || byte == 0x7FU || text[index] == '\\') {
            appendEscaped(byte, result);
        } else {
            result += text[index];
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
    text += projectNameHint();
    return text;
}

auto render(const Project::PathExists& error) -> std::string
{
    std::string text = "error: '";
    text += printablePath(error.path);
    text += "' already exists\n";
    text += "hint: choose another name, or run the command in another directory\n";
    return text;
}

/**
 * Whether @p code is the quota failure, which std::errc does not name.
 */
auto isQuotaExceeded(const std::error_code& code) -> bool
{
#ifdef EDQUOT
    return code.value() == EDQUOT
           && (code.category() == std::generic_category() || code.category() == std::system_category());
#else
    static_cast<void>(code);
    return false;
#endif
}

/**
 * The next step for a directory or file that could not be created, chosen by
 * what the operating system reported.
 */
auto cannotCreateHint(const std::error_code& code) -> std::string_view
{
    if (code == std::errc::permission_denied || code == std::errc::operation_not_permitted) {
        return PermissionHint;
    }
    if (code == std::errc::no_space_on_device || isQuotaExceeded(code)) {
        return "hint: free some disk space and run the command again\n";
    }
    if (code == std::errc::read_only_file_system) {
        return "hint: run the command in a writable directory\n";
    }
    return "hint: check that the directory exists and can be written\n";
}

auto render(const Project::CannotCreate& error) -> std::string
{
    std::string text = "error: cannot create '";
    text += printablePath(error.path);
    text += "': ";
    text += error.reason;
    text += '\n';
    if (error.leftBehind.has_value()) {
        text += "hint: remove the partly created '";
        text += printablePath(*error.leftBehind);
        text += "' before trying again\n";
    } else {
        text += cannotCreateHint(error.code);
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

auto renderCreateProjectError(const Project::CreateProjectError& error) -> std::string
{
    return std::visit(
        [](const auto& alternative) -> std::string {
            return render(alternative);
        },
        error);
}

}  // namespace scrap::Command
