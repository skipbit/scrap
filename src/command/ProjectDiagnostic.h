#pragma once

#include "build/BuildOutput.h"
#include "build/BuildStep.h"
#include "command/ProjectArgument.h"
#include "compile/CompilationDatabase.h"
#include "project/LanguageStandard.h"
#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Command {

/**
 * @brief Describe why a project could not be loaded, and what to do next.
 *
 * The first line states the cause: "error: ..." for a path or a missing
 * project, and the compiler-style line from scrap::Project::describe() for a
 * manifest. A "hint: ..." line with the next step follows.
 *
 * @param error Error returned by scrap::Project::loadProject().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderProjectError(const Project::ProjectError& error);

/**
 * @brief Describe an explicitly empty path argument, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderEmptyPathArgument();

/**
 * @brief Describe why the project of a path argument could not be loaded, and
 *        what to do next.
 *
 * @param error Error returned by scrap::Command::loadProjectAt().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderProjectArgumentError(const ProjectArgumentError& error);

/**
 * @brief Describe a system with no C++ compiler, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderNoCompilerFound();

/**
 * @brief Describe a compiler the environment asked for and cannot be run.
 *
 * @param requested The value CXX gave.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderUnusableCompilerRequest(std::string_view requested);

/**
 * @brief Describe a project that has nothing to build, and what to do next.
 *
 * @param projectRoot Directory the manifest was read from.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderNoTargetToBuild(const std::filesystem::path& projectRoot);

/**
 * @brief Describe a project with no executable to run, and what to do next.
 *
 * @param projectRoot Directory the manifest was read from.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderNoExecutableToRun(const std::filesystem::path& projectRoot);

/**
 * @brief Describe a project with more than one executable to run, and what
 *        to do next.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param names The executable targets, in the order the project states them.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderSeveralExecutablesToRun(const std::filesystem::path& projectRoot, const std::vector<std::string>& names);

/**
 * @brief Describe a target asked for by a name the project has no target of,
 *        and what to do next.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param requested The name given on the command line.
 * @param names The project's targets, in the order the project states them.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderNoTargetNamed(const std::filesystem::path& projectRoot,
                                              std::string_view requested,
                                              const std::vector<std::string>& names);

/**
 * @brief Describe an executable asked for by a name the project has no
 *        executable of, and what to do next.
 *
 * A library's name is answered the same way: it is not one of the names
 * listed.
 *
 * @param projectRoot Directory the manifest was read from.
 * @param requested The name given on the command line.
 * @param names The executable targets, in the order the project states them.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderNoExecutableNamed(const std::filesystem::path& projectRoot,
                                                  std::string_view requested,
                                                  const std::vector<std::string>& names);

/**
 * @brief Describe an executable that was built and could not be started, and
 *        what to do next.
 *
 * @param executable The executable, absolute.
 * @param code What the operating system reported.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderExecutableNotStarted(const std::filesystem::path& executable, const std::error_code& code);

/**
 * @brief Describe a source directory that could not be read, and what to do next.
 *
 * @param failure Failure returned by scrap::Project::collectSources().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderSourceScanFailure(const Project::SourceScanFailure& failure);

/**
 * @brief Describe a compilation database that could not be written, and what
 *        to do next.
 *
 * @param failure Failure returned by scrap::Compile::writeCompilationDatabase().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderCompilationDatabaseFailure(const Compile::DatabaseWriteFailure& failure);

/**
 * @brief Describe a standard the compiler in use cannot build, and what to do
 *        next.
 *
 * @param compiler The compiler that was asked for, absolute.
 * @param standard The standard scrap.toml states.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderUnsupportedStandard(const std::filesystem::path& compiler, Project::LanguageStandard standard);

/**
 * @brief Describe a project that declares more than one library, and what to
 *        do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderSeveralLibraries();

/**
 * @brief Describe a library with no sources to build it from, and what to do
 *        next.
 *
 * @param name The library target the manifest declares.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderLibraryWithoutSources(std::string_view name);

/**
 * @brief Describe an archiver the compiler names that cannot be run, and what
 *        to do next.
 *
 * @param archiver What the compiler named, as scrap::Toolchain::NoArchiver holds it.
 * @param compiler The compiler in use, absolute.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderArchiverNotFound(std::string_view archiver, const std::filesystem::path& compiler);

/**
 * @brief Describe the steps of the build that failed, and what to do next.
 *
 * The compiler has already written its own diagnostics, which say what is
 * wrong with the code; this says which files the build stopped at. An error
 * line is written for each step, followed by the hints they call for, each
 * once and in the order they first appear.
 *
 * @param failures The steps that failed, in the order they ended.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderStepFailures(const std::vector<Build::FailedStep>& failures);

/**
 * @brief Describe build output that could not be removed, and what to do next.
 *
 * @param failure Failure returned by scrap::Build::removeOutput().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderOutputRemovalFailure(const Build::OutputRemovalFailure& failure);

/**
 * @brief Describe why a project could not be created, and what to do next.
 *
 * @param error Error returned by scrap::Project::createProject().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderCreateProjectError(const Project::CreateProjectError& error);

}  // namespace scrap::Command
