#pragma once

#include "project/Manifest.h"
#include "project/ManifestError.h"

#include <expected>
#include <filesystem>
#include <string_view>

namespace scrap::Project {

/**
 * @brief Parse manifest text into a Manifest.
 *
 * Validates the [package] table and any [[bin]] / [[lib]] tables. A key the
 * current version does not know is reported as an error, which keeps a
 * misspelt declaration distinguishable from an absent one.
 *
 * @param text TOML text of the manifest.
 * @param file Path reported in errors. Not read from.
 * @return Parsed manifest, or the first error found.
 */
[[nodiscard]] std::expected<Manifest, ManifestError> parseManifest(std::string_view text, const std::filesystem::path& file);

/**
 * @brief Read a manifest file from disk and parse it.
 *
 * @param file Path to the manifest.
 * @return Parsed manifest, or an error describing why it could not be read
 *         or parsed.
 */
[[nodiscard]] std::expected<Manifest, ManifestError> loadManifest(const std::filesystem::path& file);

}  // namespace scrap::Project
