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
 * @brief What kind of failure a ManifestError reports.
 */
enum class ManifestErrorKind : std::uint8_t {
    Invalid,    ///< The contents break the manifest's syntax or rules.
    Unreadable  ///< The file could not be opened or read.
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
    ManifestErrorKind kind = ManifestErrorKind::Invalid;
};

/**
 * @brief Render an error as a single diagnostic line.
 *
 * The shape is "<file>[:<line>:<column>]: error: [<key>: ]<message>", which
 * matches what compilers emit and what editors know how to jump to.
 *
 * @param error Error to render.
 * @return Diagnostic line, without a trailing newline.
 */
[[nodiscard]] std::string describe(const ManifestError& error);

}  // namespace scrap::Project
