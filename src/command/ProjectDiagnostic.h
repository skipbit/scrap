#pragma once

#include "build/BuildOutput.h"
#include "build/SerialBuild.h"
#include "compile/CompilationDatabase.h"
#include "project/LanguageStandard.h"
#include "project/ProjectCreator.h"
#include "project/ProjectLoader.h"
#include "project/SourceCollector.h"

#include <filesystem>
#include <string>
#include <string_view>

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
 * @brief Describe a library this version does not build, and what to do next.
 *
 * @param name The library target the manifest declares.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderLibraryNotBuilt(std::string_view name);

/**
 * @brief Describe a step of the build that failed, and what to do next.
 *
 * The compiler has already written its own diagnostics, which say what is
 * wrong with the code; this says which file the build stopped at.
 *
 * @param failed The step returned by scrap::Build::runSerially().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] std::string renderStepFailure(const Build::FailedStep& failed);

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
