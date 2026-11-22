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

std::string render(const EmptyPathArgument& /*empty*/, const std::string_view command)
{
    return renderEmptyPathArgumentError() + pathHint(command);
}

std::string render(const Project::ProjectError& error, const std::string_view /*command*/)
{
    return renderProjectError(error);
}

std::string render(const PathMissing& missing, const std::string_view command)
{
    return "error: '" + printablePath(missing.path) + "' does not exist\n" + pathHint(command);
}

std::string render(const PathOutsideProject& outside, const std::string_view command)
{
    std::string text = "error: '" + printablePath(outside.path) + "' is not in the project at '" + printablePath(outside.projectRoot) + "'\n";
    text += "hint: run scrap ";
    text += command;
    text += " once for each project\n";
    return text;
}

}  // anonymous namespace

std::string renderProjectPathsError(const ProjectPathsError& error, const std::string_view command)
{
    return std::visit([command](const auto& alternative) -> std::string {
        return render(alternative, command);
    }, error);
}

std::string renderNotCovered(const std::filesystem::path& file, const std::string_view command)
{
    std::string text = "warning: leaving out '" + printablePath(file) + "', which scrap ";
    text += command;
    text += " does not cover\n";
    return text;
}

}  // namespace scrap::Command
