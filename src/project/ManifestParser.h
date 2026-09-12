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
 * Validates the [package] table and any [[bin]] / [[lib]] tables. Tables and
 * keys the current version does not know about are ignored, so a manifest
 * written for a later version still loads as far as it is understood.
 *
 * @param text TOML text of the manifest.
 * @param file Path reported in errors. Not read from.
 * @return Parsed manifest, or the first error found.
 */
[[nodiscard]] auto parseManifest(std::string_view text,
                                 const std::filesystem::path& file) -> std::expected<Manifest, ManifestError>;

/**
 * @brief Read a manifest file from disk and parse it.
 *
 * @param file Path to the manifest.
 * @return Parsed manifest, or an error describing why it could not be read
 *         or parsed.
 */
[[nodiscard]] auto loadManifest(const std::filesystem::path& file) -> std::expected<Manifest, ManifestError>;

}  // namespace scrap::Project
