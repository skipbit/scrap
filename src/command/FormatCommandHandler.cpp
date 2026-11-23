#include "command/FormatCommandHandler.h"

#include "command/FormatText.h"
#include "command/InvocationContext.h"
#include "command/ParsedOptions.h"
#include "command/ProjectDiagnostic.h"
#include "command/ProjectPaths.h"
#include "command/ProjectPathsText.h"
#include "command/ToolLookup.h"
#include "format/ClangFormat.h"
#include "project/FormattedFiles.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/TargetResolver.h"
#include "tool/ExternalTool.h"

#include <filesystem>
#include <iostream>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/// The command as the messages name it.
constexpr std::string_view CommandName = "format";

/**
 * Format @p files of @p project in place with @p tool, and return the exit
 * code.
 */
int formatFiles(const Tool::ExternalTool& tool, const Project::LoadedProject& project, const std::vector<std::filesystem::path>& files)
{
    int result = 0;
    for (const std::filesystem::path& file : files) {
        const auto formatted = Format::formatFile(tool, project.root, file, project.manifest.formatStyle);
        if (formatted.has_value()) {
            continue;
        }
        std::cerr << renderFormatFailure(tool, file, formatted.error());
        // A clang-format that cannot be started fails every file the same way.
        if (std::holds_alternative<Tool::ToolNotRun>(formatted.error())) {
            return 1;
        }
        result = 1;
    }
    return result;
}

/**
 * Report which of @p files of @p project formatting with @p tool would
 * change, leaving them as they are, and return the exit code.
 */
int checkFiles(const Tool::ExternalTool& tool, const Project::LoadedProject& project, const std::vector<std::filesystem::path>& files)
{
    int result = 0;
    bool unformatted = false;
    for (const std::filesystem::path& file : files) {
        const auto checked = Format::checkFile(tool, project.root, file, project.manifest.formatStyle);
        if (checked.has_value()) {
            if (*checked == Format::FileState::Unformatted) {
                std::cerr << renderNotFormatted(file);
                unformatted = true;
                result = 1;
            }
            continue;
        }
        std::cerr << renderCheckFailure(tool, file, checked.error());
        // A clang-format that cannot be started fails every file the same way.
        if (std::holds_alternative<Tool::ToolNotRun>(checked.error())) {
            return 1;
        }
        result = 1;
    }
    if (unformatted) {
        std::cerr << renderFormatHint();
    }
    return result;
}

}  // anonymous namespace

int FormatCommandHandler::execute(const InvocationContext& ctx)
{
    const auto loaded = loadProjectWithPaths(ctx.env->workingDirectory, ctx.options.positional);
    if (! loaded.has_value()) {
        std::cerr << renderProjectPathsError(loaded.error(), CommandName);
        return 1;
    }
    const Project::LoadedProject& project = loaded->project;

    // A project with no target still has files below src/ and include/ to
    // format.
    const auto targets = Project::resolveTargets(project.root, project.manifest).value_or(std::vector<Project::Target>{});
    const auto covered = Project::filesToFormat(project.root, targets);
    if (! covered.has_value()) {
        std::cerr << renderSourceScanFailure(covered.error());
        return 1;
    }
    const std::vector<std::filesystem::path> files = selectRequestedFiles(*covered, loaded->requested, CommandName);
    if (files.empty()) {
        return 0;
    }

    const auto tool = lookUpTool(Format::ClangFormatName, ctx.env->systemSearchPaths);
    if (! tool.has_value()) {
        return tool.error();
    }

    if (isGiven(ctx.options, CheckOption)) {
        return checkFiles(*tool, project, files);
    }
    return formatFiles(*tool, project, files);
}

}  // namespace scrap::Command
