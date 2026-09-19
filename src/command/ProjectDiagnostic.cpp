#include "command/ProjectDiagnostic.h"

#include "project/ManifestError.h"
#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"
#include "project/SourceCollector.h"
#include "project/TargetResolver.h"

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

/// The longest rejected name echoed back, counted before any escape expands it.
constexpr std::size_t EchoedNameLimit = Project::MaxProjectNameLength;

/**
 * Text the user typed, made safe to print: printable ASCII stays as it is and
 * every other byte becomes \xNN, so control characters and escape sequences
 * reach the terminal as text rather than as instructions. A backslash is
 * escaped as well, which leaves \xNN as the mark of an escaped byte alone.
 *
 * Every name long enough to be cut is already too long to be a project name,
 * so the cut costs the reader nothing and keeps one screen enough for the
 * message.
 */
auto printable(std::string_view text) -> std::string
{
    const bool cut = text.size() > EchoedNameLimit;
    std::string result;
    for (const char ch : text.substr(0, EchoedNameLimit)) {
        const unsigned byte = static_cast<unsigned char>(ch);
        if (byte >= 0x20U && byte < 0x7FU && ch != '\\') {
            result += ch;
        } else {
            appendEscaped(byte, result);
        }
    }
    if (cut) {
        result += "...";
    }
    return result;
}

/**
 * The length of the well-formed UTF-8 sequence starting at @p index, or 0 when
 * the bytes there do not form one.
 */
auto utf8SequenceLength(std::string_view text, const std::size_t index) -> std::size_t
{
    const unsigned lead = static_cast<unsigned char>(text[index]);
    std::size_t length = 0;
    if ((lead & 0xE0U) == 0xC0U) {
        length = 2;
    } else if ((lead & 0xF0U) == 0xE0U) {
        length = 3;
    } else if ((lead & 0xF8U) == 0xF0U) {
        length = 4;
    }
    if (length == 0 || index + length > text.size()) {
        return 0;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        if ((static_cast<unsigned char>(text[index + offset]) & 0xC0U) != 0x80U) {
            return 0;
        }
    }
    return length;
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
    return code.value() == EDQUOT &&
        (code.category() == std::generic_category() || code.category() == std::system_category());
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
    if (code == std::errc::invalid_argument) {
        return "hint: choose a template whose files stay inside the project\n";
    }
    if (code == std::errc::file_exists) {
        return "hint: check what is already at that path\n";
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

/**
 * Render whichever alternative @p error holds.
 */
template <typename... Alternatives> auto renderAlternative(const std::variant<Alternatives...>& error) -> std::string
{
    return std::visit(
        [](const auto& alternative) -> std::string {
            return render(alternative);
        },
        error);
}

}  // anonymous namespace

auto renderProjectError(const Project::ProjectError& error) -> std::string
{
    return renderAlternative(error);
}

/**
 * A byte outside ASCII is kept only as part of a well-formed UTF-8 sequence
 * that is not a C1 control character. A lone byte in that range is escaped,
 * since a terminal in an eight-bit locale acts on 0x80 to 0x9F as controls.
 */
auto printablePath(const std::filesystem::path& path) -> std::string
{
    const std::string text = path.string();
    std::string result;
    std::size_t index = 0;
    while (index < text.size()) {
        const unsigned byte = static_cast<unsigned char>(text[index]);
        if (byte < 0x80U) {
            if (byte < 0x20U || byte == 0x7FU || text[index] == '\\') {
                appendEscaped(byte, result);
            } else {
                result += text[index];
            }
            ++index;
            continue;
        }

        const std::size_t length = utf8SequenceLength(text, index);
        const bool isC1 = length == 2 && byte == 0xC2U && static_cast<unsigned char>(text[index + 1]) <= 0x9FU;
        if (length == 0) {
            appendEscaped(byte, result);
            ++index;
        } else if (isC1) {
            appendEscaped(byte, result);
            appendEscaped(static_cast<unsigned char>(text[index + 1]), result);
            index += 2;
        } else {
            result.append(text, index, length);
            index += length;
        }
    }
    return result;
}

auto renderEmptyPathArgument() -> std::string
{
    std::string text = "error: the path argument is empty\n";
    text += PathHint;
    return text;
}

auto renderNoCompilerFound() -> std::string
{
    return "error: no C++ compiler found\n"
           "hint: install a C++ compiler, or set CXX to the one to use\n";
}

/**
 * The value is echoed through printable(), since the environment can hold any
 * byte and the answer is read in a terminal.
 */
auto renderUnusableCompilerRequest(const std::string_view requested) -> std::string
{
    std::string text = "error: CXX names '";
    text += printable(requested);
    text += "', which cannot be run\n";
    text += "hint: set CXX to the path of a compiler, or unset it to search for one\n";
    return text;
}

auto renderNoTargetToBuild(const std::filesystem::path& projectRoot) -> std::string
{
    std::string text = "error: no target to build in '";
    text += printablePath(projectRoot);
    text += "'\nhint: add a [[bin]] or [[lib]] section to ";
    text += Project::ManifestFileName;
    text += ", or create ";
    text += Project::DefaultEntryPoint;
    text += '\n';
    return text;
}

auto renderSourceScanFailure(const Project::SourceScanFailure& failure) -> std::string
{
    std::string text = "error: cannot read '";
    text += printablePath(failure.directory);
    text += "': ";
    text += failure.reason;
    text += '\n';
    text += PermissionHint;
    return text;
}

auto renderCreateProjectError(const Project::CreateProjectError& error) -> std::string
{
    return renderAlternative(error);
}

}  // namespace scrap::Command
