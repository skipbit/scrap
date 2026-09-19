#pragma once

#include "compile/CompileCommand.h"

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Compile {

/// Name tools look for a compilation database under.
inline constexpr std::string_view CompilationDatabaseFileName = "compile_commands.json";

/**
 * @brief The step at which writing the compilation database failed.
 */
enum class DatabaseWriteStep : std::uint8_t {
    CreateDirectory,  ///< The directory to hold it could not be created.
    WriteFile         ///< The file itself could not be written.
};

/**
 * @brief The compilation database could not be written.
 */
struct DatabaseWriteFailure {
    DatabaseWriteStep step = DatabaseWriteStep::WriteFile;
    std::filesystem::path path;  ///< The directory or the file, whichever the step names.
    std::error_code code;        ///< What the operating system reported.
};

/**
 * @brief Render commands as a JSON compilation database.
 *
 * Each entry carries "directory", "file", "arguments" and "output". The
 * arguments are listed one by one, so a path holding a space or a quote is
 * written as it is rather than quoted for a shell. Strings keep their bytes,
 * apart from the escapes JSON requires.
 *
 * @param commands Commands in the order to write them.
 * @return The document, ending in a newline.
 */
[[nodiscard]] auto renderCompilationDatabase(const std::vector<CompileCommand>& commands) -> std::string;

/**
 * @brief Write the compilation database into a build directory.
 *
 * The directory is created when it is missing. The file is written under a
 * name no other file holds and then renamed into place, so a tool reading it
 * sees either the previous database or the new one whole, and two builds
 * running at once each write a file of their own.
 *
 * @param buildDirectory Directory to write into, absolute.
 * @param commands Commands to write, possibly none.
 * @return Nothing on success, or the step that failed.
 */
[[nodiscard]] auto
writeCompilationDatabase(const std::filesystem::path& buildDirectory,
                         const std::vector<CompileCommand>& commands) -> std::expected<void, DatabaseWriteFailure>;

}  // namespace scrap::Compile
