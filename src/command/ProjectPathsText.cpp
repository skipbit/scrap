#include "command/ProjectPathsText.h"

#include "command/PrintableText.h"
#include "command/ProjectArgument.h"
#include "command/ProjectDiagnostic.h"
#include "command/ProjectPaths.h"
#include "project/ProjectLoader.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

namespace {

/// What to pass instead of a path that names nothing.
std::string pathHint(const std::string_view command)
{
    std::string text = "hint: pass a file or directory inside the project, or omit the path to ";
    text += command;
    text += " the whole project\n";
    return text;
}

}  // anonymous namespace

std::string renderProjectPathsError(const ProjectPathsError& error, const std::string_view command)
{
    if (std::holds_alternative<EmptyPathArgument>(error)) {
        return "error: the path argument is empty\n" + pathHint(command);
    }
    if (const auto* const missing = std::get_if<PathMissing>(&error)) {
        return "error: '" + printablePath(missing->path) + "' does not exist\n" + pathHint(command);
    }
    if (const auto* const outside = std::get_if<PathOutsideProject>(&error)) {
        std::string text = "error: '" + printablePath(outside->path) + "' is not in the project at '" + printablePath(outside->projectRoot) + "'\n";
        text += "hint: run scrap ";
        text += command;
        text += " once for each project\n";
        return text;
    }
    return renderProjectError(std::get<Project::ProjectError>(error));
}

std::string renderNotCovered(const std::filesystem::path& file, const std::string_view command)
{
    std::string text = "warning: leaving out '" + printablePath(file) + "', which scrap ";
    text += command;
    text += " does not cover\n";
    return text;
}

}  // namespace scrap::Command
