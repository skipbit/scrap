#pragma once

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
[[nodiscard]] auto renderProjectError(const Project::ProjectError& error) -> std::string;

/**
 * @brief Describe an explicitly empty path argument, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderEmptyPathArgument() -> std::string;

/**
 * @brief Describe a system with no C++ compiler, and what to do next.
 *
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderNoCompilerFound() -> std::string;

/**
 * @brief Describe a compiler the environment asked for and cannot be run.
 *
 * @param requested The value CXX gave.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderUnusableCompilerRequest(std::string_view requested) -> std::string;

/**
 * @brief Describe a project that has nothing to build, and what to do next.
 *
 * @param projectRoot Directory the manifest was read from.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderNoTargetToBuild(const std::filesystem::path& projectRoot) -> std::string;

/**
 * @brief Describe a source directory that could not be read, and what to do next.
 *
 * @param failure Failure returned by scrap::Project::collectSources().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderSourceScanFailure(const Project::SourceScanFailure& failure) -> std::string;

/**
 * @brief Describe a compilation database that could not be written, and what
 *        to do next.
 *
 * @param failure Failure returned by scrap::Compile::writeCompilationDatabase().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderCompilationDatabaseFailure(const Compile::DatabaseWriteFailure& failure) -> std::string;

/**
 * @brief Describe a standard the compiler in use cannot build, and what to do
 *        next.
 *
 * @param compiler The compiler that was asked for, absolute.
 * @param standard The standard scrap.toml states.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderUnsupportedStandard(const std::filesystem::path& compiler, Project::LanguageStandard standard) -> std::string;

/**
 * @brief Describe a library this version does not build, and what to do next.
 *
 * @param name The library target the manifest declares.
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderLibraryNotBuilt(std::string_view name) -> std::string;

/**
 * @brief Describe a step of the build that failed, and what to do next.
 *
 * The compiler has already written its own diagnostics, which say what is
 * wrong with the code; this says which file the build stopped at.
 *
 * @param failed The step returned by scrap::Build::runSerially().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderStepFailure(const Build::FailedStep& failed) -> std::string;

/**
 * @brief Describe why a project could not be created, and what to do next.
 *
 * @param error Error returned by scrap::Project::createProject().
 * @return Text for standard error, each line ending in a newline.
 */
[[nodiscard]] auto renderCreateProjectError(const Project::CreateProjectError& error) -> std::string;

}  // namespace scrap::Command
