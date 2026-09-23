#include "command/ProjectDiagnostic.h"

#include "build/BuildStep.h"
#include "build/SerialBuild.h"
#include "command/PrintableText.h"
#include "compile/CompilationDatabase.h"
#include "project/LanguageStandard.h"
#include "project/ManifestError.h"
#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"
#include "project/ProjectLocator.h"
#include "project/SourceCollector.h"
#include "project/TargetResolver.h"

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace scrap::Command {

namespace {

/// Next step for a path argument that cannot be searched from.
constexpr std::string_view PathHint = "hint: pass a directory inside a project, or omit the path to use the current directory\n";

/// Next step for a path the operating system refused.
constexpr std::string_view PermissionHint = "hint: check the permissions of the path\n";

/// Next step for a disk or a quota that is full.
constexpr std::string_view DiskSpaceHint = "hint: free some disk space and run the command again\n";

/// Next step for a path taken by something else.
constexpr std::string_view ExistingPathHint = "hint: check what is already at that path\n";

/// Next step after a compiler diagnostic, which already says what is wrong.
constexpr std::string_view FixErrorsHint = "hint: fix the errors reported above and run the command again\n";

/// Next step when a signal stopped the compiler, which running out of memory does.
constexpr std::string_view CompilerMemoryHint = "hint: check that the compiler has enough memory and run the command again\n";

/// Next step when the compiler found earlier could not be started.
constexpr std::string_view CompilerRunHint = "hint: check that the compiler can be run, or set CXX to another one\n";

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

/// The longest value echoed back, counted before any escape expands it.
constexpr std::size_t EchoedNameLimit = Project::MaxProjectNameLength;

/**
 * A value the user gave, made safe to print and cut where it is longer than
 * a name can be.
 *
 * Every value long enough to be cut is already too long to be the name it
 * was given as, so the cut costs the reader nothing and keeps one screen
 * enough for the message. A target name is not cut: it names something the
 * manifest declares, and a message has to name it as the manifest does.
 */
auto printableEcho(const std::string_view text) -> std::string
{
    if (text.size() <= EchoedNameLimit) {
        return printableName(text);
    }
    return printableName(text.substr(0, EchoedNameLimit)) + "...";
}

auto render(const Project::InvalidProjectName& error) -> std::string
{
    std::string text;
    if (error.name.empty()) {
        text = "error: the project name is empty\n";
    } else {
        text = "error: '";
        text += printableEcho(error.name);
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
    return code.value() == EDQUOT && (code.category() == std::generic_category() || code.category() == std::system_category());
#else
    static_cast<void>(code);
    return false;
#endif
}

/**
 * The next step every write failure shares: the permissions, or the space
 * left. Any other reason is left to the caller.
 */
auto sharedWriteHint(const std::error_code& code) -> std::optional<std::string_view>
{
    if (code == std::errc::permission_denied || code == std::errc::operation_not_permitted) {
        return PermissionHint;
    }
    if (code == std::errc::no_space_on_device || isQuotaExceeded(code)) {
        return DiskSpaceHint;
    }
    return std::nullopt;
}

/**
 * The next step for a directory or file that could not be created, chosen by
 * what the operating system reported.
 */
auto cannotCreateHint(const std::error_code& code) -> std::string_view
{
    if (const auto shared = sharedWriteHint(code)) {
        return *shared;
    }
    if (code == std::errc::read_only_file_system) {
        return "hint: run the command in a writable directory\n";
    }
    if (code == std::errc::invalid_argument) {
        return "hint: choose a template whose files stay inside the project\n";
    }
    if (code == std::errc::file_exists) {
        return ExistingPathHint;
    }
    return "hint: check that the directory exists and can be written\n";
}

/**
 * The next step for a build output that could not be written, chosen by what
 * the operating system reported. The output goes inside the project, so the
 * last resort points at the project directory.
 */
auto cannotWriteBuildHint(const std::error_code& code) -> std::string_view
{
    if (const auto shared = sharedWriteHint(code)) {
        return *shared;
    }
    if (code == std::errc::not_a_directory || code == std::errc::file_exists || code == std::errc::is_a_directory) {
        return ExistingPathHint;
    }
    return "hint: check that the project directory can be written\n";
}

/**
 * What the failed step was doing to its path, in the words the output uses.
 */
auto describeStep(const Compile::DatabaseWriteStep step) -> std::string_view
{
    switch (step) {
    case Compile::DatabaseWriteStep::CreateDirectory:
        return "create";
    case Compile::DatabaseWriteStep::WriteFile:
        return "write";
    }
    // Every step is answered above, so a step added without a word here fails
    // the build rather than being described as one of the others.
    std::unreachable();
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
template <typename... Alternatives>
auto renderAlternative(const std::variant<Alternatives...>& error) -> std::string
{
    return std::visit([](const auto& alternative) -> std::string {
        return render(alternative);
    }, error);
}

}  // anonymous namespace

auto renderProjectError(const Project::ProjectError& error) -> std::string
{
    return renderAlternative(error);
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
 * The value is echoed through printableEcho(), since the environment can hold any
 * byte and the answer is read in a terminal.
 */
auto renderUnusableCompilerRequest(const std::string_view requested) -> std::string
{
    std::string text = "error: CXX names '";
    text += printableEcho(requested);
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

auto renderCompilationDatabaseFailure(const Compile::DatabaseWriteFailure& failure) -> std::string
{
    std::string text = "error: cannot ";
    text += describeStep(failure.step);
    text += " '";
    text += printablePath(failure.path);
    text += "': ";
    text += failure.code.message();
    text += '\n';
    text += cannotWriteBuildHint(failure.code);
    return text;
}

namespace {

/**
 * The first line of a step that failed: which file the build stopped at, and
 * for a compilation, the target it was building.
 */
auto describeFailedStep(const Build::BuildStep& step) -> std::string
{
    if (step.kind == Build::StepKind::Link) {
        std::string text = "error: failed to link '";
        text += printablePath((step.directory / step.output).lexically_normal());
        text += '\'';
        return text;
    }
    std::string text = "error: failed to compile '";
    text += printablePath((step.directory / step.subject).lexically_normal());
    text += "' for '";
    text += printableName(step.target);
    text += '\'';
    return text;
}

}  // anonymous namespace

auto renderUnsupportedStandard(const std::filesystem::path& compiler, const Project::LanguageStandard standard) -> std::string
{
    std::string text = "error: '";
    text += printablePath(compiler);
    text += "' does not support C++";
    text += Project::standardNumber(standard);
    text += "\nhint: use a newer compiler, or set std in ";
    text += Project::ManifestFileName;
    text += " to an older standard\n";
    return text;
}

auto renderLibraryNotBuilt(const std::string_view name) -> std::string
{
    std::string text = "error: building the library '";
    text += printableName(name);
    text += "' is not supported yet\nhint: remove the [[lib]] section from ";
    text += Project::ManifestFileName;
    text += " to build its sources into the executable\n";
    return text;
}

auto renderStepFailure(const Build::FailedStep& failed) -> std::string
{
    switch (failed.failure.kind) {
    case Build::StepFailureKind::CannotCreateDirectory: {
        std::string text = "error: cannot create '";
        text += printablePath(failed.failure.path);
        text += "': ";
        text += failed.failure.code.message();
        text += '\n';
        text += cannotWriteBuildHint(failed.failure.code);
        return text;
    }
    case Build::StepFailureKind::CannotStart: {
        std::string text = "error: cannot run '";
        text += printablePath(failed.failure.path);
        text += "': ";
        text += failed.failure.code.message();
        text += '\n';
        text += CompilerRunHint;
        return text;
    }
    case Build::StepFailureKind::Signalled: {
        std::string text = describeFailedStep(failed.step);
        text += ": the compiler was stopped by signal ";
        text += std::to_string(failed.failure.status);
        text += '\n';
        text += CompilerMemoryHint;
        return text;
    }
    case Build::StepFailureKind::Exited:
        return describeFailedStep(failed.step) + '\n' + std::string{ FixErrorsHint };
    }
    // Every kind is answered above, so a kind added without a message here
    // fails the build rather than being reported as one of the others.
    std::unreachable();
}

auto renderCreateProjectError(const Project::CreateProjectError& error) -> std::string
{
    return renderAlternative(error);
}

}  // namespace scrap::Command
