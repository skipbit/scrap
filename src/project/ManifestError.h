#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace scrap::Project {

/**
 * @brief A one-based position inside the manifest file.
 */
struct SourcePosition {
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

/**
 * @brief Why a manifest could not be turned into a Manifest.
 *
 * Carries enough to point the user at the offending spot: the file, the
 * position inside it when one is known, and the dotted key the problem
 * belongs to. Syntax errors have a position but no key; a file that cannot
 * be read has neither.
 */
struct ManifestError {
    std::filesystem::path file;
    std::optional<SourcePosition> position;
    std::string key;  ///< Dotted key path, e.g. "package.name". Empty when not tied to one key.
    std::string message;
};

/**
 * @brief Render an error as a single diagnostic line.
 *
 * The shape is "<file>[:<line>:<column>]: [<key>: ]<message>", which matches
 * what compilers emit and what editors know how to jump to.
 *
 * @param error Error to render.
 * @return Diagnostic line, without a trailing newline.
 */
[[nodiscard]] auto describe(const ManifestError& error) -> std::string;

}  // namespace scrap::Project
