#include "project/ManifestParser.h"

#include "project/Manifest.h"
#include "project/ManifestError.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>  // NOLINT(misc-include-cleaner) — provides std::expected return type
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Project {

namespace {

/// Language standard assumed when [package] does not state one.
constexpr std::string_view DefaultStandard = "23";

/// Top-level keys this version recognises.
constexpr std::array<std::string_view, 6> KnownTopLevelKeys{"package",
                                                            "bin",
                                                            "lib",
                                                            "dependencies",
                                                            "toolchain",
                                                            "scripts"};

/// Keys the [package] table recognises.
constexpr std::array<std::string_view, 3> KnownPackageKeys{"name", "version", "std"};

/// Keys one [[bin]] or [[lib]] entry recognises.
constexpr std::array<std::string_view, 2> KnownTargetKeys{"name", "src"};

/**
 * A string read out of the manifest, kept with the node it came from so a
 * later check can point at where the user wrote it.
 */
struct StringField {
    std::string value;
    const toml::node* node = nullptr;
};

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
 * Read @p node as a non-empty string.
 */
auto readString(const std::filesystem::path& file,
                const toml::node& node,
                std::string key) -> std::expected<StringField, ManifestError>
{
    const auto value = node.value<std::string>();
    if (! value.has_value()) {
        return std::unexpected(errorAt(file, node, std::move(key), "must be a string"));
    }
    if (value->empty()) {
        return std::unexpected(errorAt(file, node, std::move(key), "must not be empty"));
    }
    return StringField{.value = *value, .node = &node};
}

/**
 * Read a string key that must be present.
 */
auto requireString(const std::filesystem::path& file,
                   const toml::table& table,
                   std::string_view tableName,
                   std::string_view key) -> std::expected<StringField, ManifestError>
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::unexpected(errorAt(file, table, dotted(tableName, key), "required key is missing"));
    }
    return readString(file, *node, dotted(tableName, key));
}

/**
 * Read a string key that may be absent, falling back to @p fallback.
 */
auto optionalString(const std::filesystem::path& file,
                    const toml::table& table,
                    std::string_view tableName,
                    std::string_view key,
                    std::string_view fallback) -> std::expected<StringField, ManifestError>
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return StringField{.value = std::string{fallback}, .node = nullptr};
    }
    return readString(file, *node, dotted(tableName, key));
}

/**
 * True when @p text holds a character that has no place in a file name.
 */
auto hasControlCharacter(std::string_view text) -> bool
{
    return std::ranges::any_of(text, [](const char ch) {
        return static_cast<unsigned char>(ch) < 0x20;
    });
}

/**
 * Reject a name that cannot stand as an artifact name on disk.
 *
 * A name reaches the filesystem as the last component of an output path, so
 * one carrying a separator or a parent-directory reference would place the
 * artifact outside the directory the caller chose.
 */
auto validateName(const std::filesystem::path& file,
                  const StringField& field,
                  std::string key) -> std::expected<void, ManifestError>
{
    if (field.node == nullptr) {
        return {};
    }
    if (field.value == "." || field.value == "..") {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not be '.' or '..'"));
    }
    if (field.value.find('/') != std::string::npos || field.value.find('\\') != std::string::npos) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not contain a path separator"));
    }
    if (hasControlCharacter(field.value)) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not contain a control character"));
    }
    return {};
}

/**
 * Reject an entry point that does not stay inside the project.
 */
auto validateEntryPoint(const std::filesystem::path& file,
                        const StringField& field,
                        std::string key) -> std::expected<void, ManifestError>
{
    const std::filesystem::path entryPoint{field.value};
    if (entryPoint.is_absolute() || entryPoint.has_root_name() || entryPoint.has_root_directory()) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must be relative to the project root"));
    }
    for (const std::filesystem::path& part : entryPoint) {
        if (part == "..") {
            return std::unexpected(errorAt(file, *field.node, std::move(key), "must not leave the project root"));
        }
    }
    if (hasControlCharacter(field.value)) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not contain a control character"));
    }
    return {};
}

/**
 * Reject a key this version does not recognise.
 *
 * Ignoring it would turn a typo into silence: a manifest that says [[bins]],
 * or that writes bin = [] below the [package] header so the key lands inside
 * that table, would load as though the declaration had never been written and
 * be built from the default layout without a word.
 *
 * @param file Path reported in errors.
 * @param table Table to check.
 * @param prefix Dotted prefix for reported keys; empty at the top level.
 * @param known Keys this version reads or reserves.
 */
auto rejectUnknownKeys(const std::filesystem::path& file,
                       const toml::table& table,
                       std::string_view prefix,
                       std::span<const std::string_view> known) -> std::expected<void, ManifestError>
{
    for (const auto& [key, value] : table) {
        const std::string_view name = key.str();
        if (std::ranges::find(known, name) != known.end()) {
            continue;
        }

        std::string message = "unknown key; expected one of";
        for (const std::string_view candidate : known) {
            message += ' ';
            message += candidate;
        }
        std::string reported = prefix.empty() ? std::string{name} : dotted(prefix, name);
        return std::unexpected(errorAt(file, value, std::move(reported), std::move(message)));
    }
    return {};
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

    if (auto known = rejectUnknownKeys(file, *table, "package", KnownPackageKeys); ! known.has_value()) {
        return std::unexpected(known.error());
    }

    auto name = requireString(file, *table, "package", "name");
    if (! name.has_value()) {
        return std::unexpected(name.error());
    }
    // The package name becomes an artifact name when no target is declared,
    // so it is held to the same rule as a declared target name.
    if (auto valid = validateName(file, *name, "package.name"); ! valid.has_value()) {
        return std::unexpected(valid.error());
    }
    auto version = requireString(file, *table, "package", "version");
    if (! version.has_value()) {
        return std::unexpected(version.error());
    }
    auto standard = optionalString(file, *table, "package", "std", DefaultStandard);
    if (! standard.has_value()) {
        return std::unexpected(standard.error());
    }

    return Package{
        .name = std::move(name->value), .version = std::move(version->value), .standard = std::move(standard->value)};
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
    if (auto known = rejectUnknownKeys(file, table, key, KnownTargetKeys); ! known.has_value()) {
        return std::unexpected(known.error());
    }

    auto name = requireString(file, table, key, "name");
    if (! name.has_value()) {
        return std::unexpected(name.error());
    }
    if (auto valid = validateName(file, *name, dotted(key, "name")); ! valid.has_value()) {
        return std::unexpected(valid.error());
    }
    for (const Target& existing : targets) {
        if (existing.name == name->value) {
            return std::unexpected(errorAt(file, *name->node, dotted(key, "name"), "duplicate target name"));
        }
    }

    auto source = requireString(file, table, key, "src");
    if (! source.has_value()) {
        return std::unexpected(source.error());
    }
    if (auto valid = validateEntryPoint(file, *source, dotted(key, "src")); ! valid.has_value()) {
        return std::unexpected(valid.error());
    }

    targets.push_back(Target{.kind = kind, .name = std::move(name->value), .entryPoint = source->value});
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

/**
 * Read the whole of @p file, distinguishing a failure to read from an empty
 * file.
 *
 * Streaming through a stringstream cannot tell those apart: inserting from a
 * streambuf reports on the destination, never on the source, so a directory
 * opened as a manifest and a zero-byte manifest both arrive as empty text.
 * Reading a known length and comparing what arrived keeps them apart.
 */
auto readWholeFile(const std::filesystem::path& file) -> std::expected<std::string, ManifestError>
{
    std::error_code ec;
    if (! std::filesystem::is_regular_file(file, ec)) {
        return std::unexpected(errorWithoutPosition(file, {}, "cannot open the manifest"));
    }
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) {
        return std::unexpected(errorWithoutPosition(file, {}, "cannot read the manifest"));
    }

    std::ifstream input(file, std::ios::binary);
    if (! input.is_open()) {
        return std::unexpected(errorWithoutPosition(file, {}, "cannot open the manifest"));
    }
    if (size == 0) {
        return std::string{};
    }

    std::string text(static_cast<std::size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if (static_cast<std::uintmax_t>(input.gcount()) != size) {
        return std::unexpected(errorWithoutPosition(file, {}, "cannot read the manifest"));
    }
    return text;
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

    if (auto known = rejectUnknownKeys(file, parsed.table(), {}, KnownTopLevelKeys); ! known.has_value()) {
        return std::unexpected(known.error());
    }

    auto package = parsePackage(file, parsed.table());
    if (! package.has_value()) {
        return std::unexpected(package.error());
    }

    Manifest manifest;
    manifest.package = std::move(*package);
    manifest.declaresTargets = parsed.table().get("bin") != nullptr || parsed.table().get("lib") != nullptr;

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
    const auto text = readWholeFile(file);
    if (! text.has_value()) {
        return std::unexpected(text.error());
    }
    return parseManifest(*text, file);
}

}  // namespace scrap::Project
