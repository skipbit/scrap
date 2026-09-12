#include "project/ManifestParser.h"

#include "project/Manifest.h"
#include "project/ManifestError.h"

#include <toml++/toml.hpp>

#include <cstdint>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Project {

namespace {

/// Language standard assumed when [package] does not state one.
constexpr std::string_view DefaultStandard = "23";

/**
 * Convert a toml++ source position to the one reported to the user.
 */
auto toPosition(const toml::source_position& from) -> SourcePosition
{
    return SourcePosition{.line = static_cast<std::uint32_t>(from.line),
                          .column = static_cast<std::uint32_t>(from.column)};
}

/**
 * Join a table name and a key into the dotted form used in diagnostics.
 */
auto dotted(std::string_view table, std::string_view key) -> std::string
{
    std::string joined{table};
    joined += '.';
    joined += key;
    return joined;
}

/**
 * Build an error pointing at where @p node starts.
 */
auto errorAt(const std::filesystem::path& file,
             const toml::node& node,
             std::string key,
             std::string message) -> ManifestError
{
    return ManifestError{.file = file,
                         .position = toPosition(node.source().begin),
                         .key = std::move(key),
                         .message = std::move(message)};
}

/**
 * Build an error that cannot be tied to a position in the file.
 */
auto errorWithoutPosition(const std::filesystem::path& file, std::string key, std::string message) -> ManifestError
{
    return ManifestError{.file = file, .position = std::nullopt, .key = std::move(key), .message = std::move(message)};
}

/**
 * Read a string key that must be present and non-empty.
 */
auto requireString(const std::filesystem::path& file,
                   const toml::table& table,
                   std::string_view tableName,
                   std::string_view key) -> std::expected<std::string, ManifestError>
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::unexpected(errorAt(file, table, dotted(tableName, key), "required key is missing"));
    }
    const auto value = node->value<std::string>();
    if (! value.has_value()) {
        return std::unexpected(errorAt(file, *node, dotted(tableName, key), "must be a string"));
    }
    if (value->empty()) {
        return std::unexpected(errorAt(file, *node, dotted(tableName, key), "must not be empty"));
    }
    return *value;
}

/**
 * Read a string key that may be absent, falling back to @p fallback.
 */
auto optionalString(const std::filesystem::path& file,
                    const toml::table& table,
                    std::string_view tableName,
                    std::string_view key,
                    std::string_view fallback) -> std::expected<std::string, ManifestError>
{
    if (table.get(key) == nullptr) {
        return std::string{fallback};
    }
    return requireString(file, table, tableName, key);
}

/**
 * Parse the [package] table.
 */
auto parsePackage(const std::filesystem::path& file, const toml::table& root) -> std::expected<Package, ManifestError>
{
    const toml::node* node = root.get("package");
    if (node == nullptr) {
        return std::unexpected(errorWithoutPosition(file, "package", "required table is missing"));
    }
    const toml::table* table = node->as_table();
    if (table == nullptr) {
        return std::unexpected(errorAt(file, *node, "package", "must be a table"));
    }

    auto name = requireString(file, *table, "package", "name");
    if (! name.has_value()) {
        return std::unexpected(name.error());
    }
    auto version = requireString(file, *table, "package", "version");
    if (! version.has_value()) {
        return std::unexpected(version.error());
    }
    auto standard = optionalString(file, *table, "package", "std", DefaultStandard);
    if (! standard.has_value()) {
        return std::unexpected(standard.error());
    }

    return Package{.name = std::move(*name), .version = std::move(*version), .standard = std::move(*standard)};
}

/**
 * Parse one [[bin]] or [[lib]] entry and append it to @p targets.
 */
auto parseTargetEntry(const std::filesystem::path& file,
                      const toml::table& table,
                      std::string_view key,
                      TargetKind kind,
                      std::vector<Target>& targets) -> std::expected<void, ManifestError>
{
    auto name = requireString(file, table, key, "name");
    if (! name.has_value()) {
        return std::unexpected(name.error());
    }
    auto source = requireString(file, table, key, "src");
    if (! source.has_value()) {
        return std::unexpected(source.error());
    }
    targets.push_back(Target{.kind = kind, .name = std::move(*name), .entryPoint = std::move(*source)});
    return {};
}

/**
 * Parse the array of tables named @p key, if the manifest has one.
 */
auto parseTargetArray(const std::filesystem::path& file,
                      const toml::table& root,
                      std::string_view key,
                      TargetKind kind,
                      std::vector<Target>& targets) -> std::expected<void, ManifestError>
{
    const toml::node* node = root.get(key);
    if (node == nullptr) {
        return {};
    }
    const toml::array* entries = node->as_array();
    if (entries == nullptr) {
        return std::unexpected(errorAt(file, *node, std::string{key}, "must be an array of tables"));
    }

    for (const toml::node& entry : *entries) {
        const toml::table* table = entry.as_table();
        if (table == nullptr) {
            return std::unexpected(errorAt(file, entry, std::string{key}, "must be an array of tables"));
        }
        auto parsed = parseTargetEntry(file, *table, key, kind, targets);
        if (! parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
    }
    return {};
}

}  // anonymous namespace

/**
 * Parse manifest text into a Manifest.
 */
auto parseManifest(std::string_view text, const std::filesystem::path& file) -> std::expected<Manifest, ManifestError>
{
    const toml::parse_result parsed = toml::parse(text);
    if (! parsed) {
        const toml::parse_error& error = parsed.error();
        return std::unexpected(ManifestError{.file = file,
                                             .position = toPosition(error.source().begin),
                                             .key = {},
                                             .message = std::string{error.description()}});
    }

    auto package = parsePackage(file, parsed.table());
    if (! package.has_value()) {
        return std::unexpected(package.error());
    }

    Manifest manifest;
    manifest.package = std::move(*package);

    auto executables = parseTargetArray(file, parsed.table(), "bin", TargetKind::Executable, manifest.targets);
    if (! executables.has_value()) {
        return std::unexpected(executables.error());
    }
    auto libraries = parseTargetArray(file, parsed.table(), "lib", TargetKind::Library, manifest.targets);
    if (! libraries.has_value()) {
        return std::unexpected(libraries.error());
    }

    return manifest;
}

/**
 * Read a manifest file from disk and parse it.
 */
auto loadManifest(const std::filesystem::path& file) -> std::expected<Manifest, ManifestError>
{
    const std::ifstream input(file, std::ios::binary);
    if (! input.is_open()) {
        return std::unexpected(errorWithoutPosition(file, {}, "cannot open the manifest"));
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad()) {
        return std::unexpected(errorWithoutPosition(file, {}, "cannot read the manifest"));
    }

    const std::string text = buffer.str();
    return parseManifest(text, file);
}

}  // namespace scrap::Project
