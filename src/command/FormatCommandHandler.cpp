#include "command/FormatCommandHandler.h"

#include "command/FormatText.h"
#include "command/InvocationContext.h"
#include "command/ProjectDiagnostic.h"
#include "command/ProjectPaths.h"
#include "command/ProjectPathsText.h"
#include "format/ClangFormat.h"
#include "project/FileSelection.h"
#include "project/FormattedFiles.h"
#include "project/Manifest.h"
#include "project/ProjectLoader.h"
#include "project/TargetResolver.h"

#include <filesystem>
#include <iostream>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Command {

namespace {

/// The command as the messages name it.
constexpr std::string_view CommandName = "format";

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
    const Project::FileSelection selection = Project::selectFiles(*covered, loaded->requested);
    for (const std::filesystem::path& file : selection.leftOut) {
        std::cerr << renderNotCovered(file, CommandName);
    }
    for (const std::filesystem::path& directory : selection.emptyDirectories) {
        std::cerr << renderNothingCoveredBelow(directory, CommandName);
    }
    if (selection.selected.empty()) {
        return 0;
    }

    const auto program = Format::findClangFormat(ctx.env->systemSearchPaths);
    if (! program.has_value()) {
        std::cerr << renderClangFormatNotFound();
        return 1;
    }
    const auto tool = Format::describeClangFormat(*program);
    if (! tool.has_value()) {
        std::cerr << renderVersionFailure(*program, tool.error());
        return 1;
    }
    std::cerr << renderClangFormatInUse(*tool);

    int result = 0;
    for (const std::filesystem::path& file : selection.selected) {
        const auto formatted = Format::formatFile(*tool, project.root, file, project.manifest.formatStyle);
        if (formatted.has_value()) {
            continue;
        }
        std::cerr << renderFormatFailure(*tool, file, formatted.error());
        // A clang-format that cannot be started fails every file the same way.
        if (std::holds_alternative<Format::ToolNotRun>(formatted.error())) {
            return 1;
        }
        result = 1;
    }
    return result;
}

}  // namespace scrap::Command
