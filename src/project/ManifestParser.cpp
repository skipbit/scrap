#include "project/ManifestParser.h"

#include "project/LanguageStandard.h"
#include "project/Manifest.h"
#include "project/ManifestError.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
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
constexpr LanguageStandard DefaultStandard = LanguageStandard::Cxx23;

/// Top-level keys this version recognises.
constexpr std::array<std::string_view, 6> KnownTopLevelKeys{ "package", "bin", "lib", "dependencies", "toolchain", "scripts" };

/// Keys the [package] table recognises.
constexpr std::array<std::string_view, 3> KnownPackageKeys{ "name", "version", "std" };

/// Keys one [[bin]] entry recognises.
constexpr std::array<std::string_view, 6> KnownExecutableKeys{ "name", "src", "include-dirs", "defines", "compile-flags", "link-flags" };

/// Keys one [[lib]] entry recognises. A static library is not linked, so
/// link-flags belong under its public table only.
constexpr std::array<std::string_view, 6> KnownLibraryKeys{ "name", "src", "include-dirs", "defines", "compile-flags", "public" };

/// Keys the public table of a [[lib]] entry recognises.
constexpr std::array<std::string_view, 4> KnownPublicKeys{ "include-dirs", "defines", "compile-flags", "link-flags" };

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
SourcePosition toPosition(const toml::source_position& from)
{
    return SourcePosition{ .line = static_cast<std::uint32_t>(from.line), .column = static_cast<std::uint32_t>(from.column) };
}

/**
 * Join a table name and a key into the dotted form used in diagnostics.
 */
std::string dotted(std::string_view table, std::string_view key)
{
    std::string joined{ table };
    joined += '.';
    joined += key;
    return joined;
}

/**
 * Build an error pointing at where @p node starts.
 */
ManifestError errorAt(const std::filesystem::path& file, const toml::node& node, std::string key, std::string message)
{
    return ManifestError{
        .file = file,
        .position = toPosition(node.source().begin),
        .key = std::move(key),
        .message = std::move(message),
        .kind = ManifestErrorKind::Invalid
    };
}

/**
 * Build an error that cannot be tied to a position in the file.
 */
ManifestError errorWithoutPosition(const std::filesystem::path& file, std::string key, std::string message)
{
    return ManifestError{
        .file = file, .position = std::nullopt, .key = std::move(key), .message = std::move(message), .kind = ManifestErrorKind::Invalid
    };
}

/**
 * Build an error for a manifest file that could not be opened or read.
 */
ManifestError unreadable(const std::filesystem::path& file, std::string message)
{
    return ManifestError{
        .file = file, .position = std::nullopt, .key = {}, .message = std::move(message), .kind = ManifestErrorKind::Unreadable
    };
}

/**
 * Read @p node as a non-empty string.
 */
std::expected<StringField, ManifestError> readString(const std::filesystem::path& file, const toml::node& node, std::string key)
{
    const auto value = node.value<std::string>();
    if (! value.has_value()) {
        return std::unexpected(errorAt(file, node, std::move(key), "must be a string"));
    }
    if (value->empty()) {
        return std::unexpected(errorAt(file, node, std::move(key), "must not be empty"));
    }
    return StringField{ .value = *value, .node = &node };
}

/**
 * Read a string key that must be present.
 */
std::expected<StringField, ManifestError> requireString(const std::filesystem::path& file,
                                                        const toml::table& table,
                                                        std::string_view tableName,
                                                        std::string_view key)
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::unexpected(errorAt(file, table, dotted(tableName, key), "required key is missing"));
    }
    return readString(file, *node, dotted(tableName, key));
}

/**
 * Read package.std, falling back to the default standard when it is absent.
 *
 * A value naming no standard this version accepts, "gnu23" among them, is
 * reported here: handed to the compiler, it would be rejected without saying
 * what to write instead.
 */
std::expected<LanguageStandard, ManifestError> parseStandard(const std::filesystem::path& file, const toml::table& package)
{
    const toml::node* node = package.get("std");
    if (node == nullptr) {
        return DefaultStandard;
    }
    auto field = readString(file, *node, "package.std");
    if (! field.has_value()) {
        return std::unexpected(field.error());
    }
    if (const auto standard = parseLanguageStandard(field->value); standard.has_value()) {
        return *standard;
    }

    std::string message = "unsupported standard; expected one of";
    std::string_view separator = " ";
    for (const LanguageStandard candidate : supportedLanguageStandards()) {
        message += separator;
        message += '"';
        message += standardNumber(candidate);
        message += '"';
        separator = ", ";
    }
    return std::unexpected(errorAt(file, *node, "package.std", std::move(message)));
}

/**
 * True when @p text holds a character that has no place in a file name.
 */
bool hasControlCharacter(std::string_view text)
{
    return std::ranges::any_of(text, [](const char ch) {
        return (static_cast<unsigned char>(ch) < 0x20);
    });
}

/**
 * Reject a name that cannot stand as an artifact name on disk.
 *
 * A name reaches the filesystem as the last component of an output path, so
 * one carrying a separator or a parent-directory reference would place the
 * artifact outside the directory the caller chose.
 */
std::expected<void, ManifestError> validateName(const std::filesystem::path& file, const StringField& field, std::string key)
{
    if (field.node == nullptr) {
        return {};
    }
    if (field.value == "." || field.value == "..") {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not be '.' or '..'"));
    }
    if ((field.value.find('/') != std::string::npos) || (field.value.find('\\') != std::string::npos)) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not contain a path separator"));
    }
    if (hasControlCharacter(field.value)) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must not contain a control character"));
    }
    return {};
}

/**
 * Reject a path that does not stay inside the project.
 */
std::expected<void, ManifestError> validateProjectPath(const std::filesystem::path& file, const StringField& field, std::string key)
{
    const std::filesystem::path path{ field.value };
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return std::unexpected(errorAt(file, *field.node, std::move(key), "must be relative to the project root"));
    }
    for (const std::filesystem::path& part : path) {
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
std::expected<void, ManifestError> rejectUnknownKeys(const std::filesystem::path& file,
                                                     const toml::table& table,
                                                     std::string_view prefix,
                                                     std::span<const std::string_view> known)
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
        std::string reported = prefix.empty() ? std::string{ name } : dotted(prefix, name);
        return std::unexpected(errorAt(file, value, std::move(reported), std::move(message)));
    }
    return {};
}

/**
 * Parse the [package] table.
 */
std::expected<Package, ManifestError> parsePackage(const std::filesystem::path& file, const toml::table& root)
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
    auto standard = parseStandard(file, *table);
    if (! standard.has_value()) {
        return std::unexpected(standard.error());
    }

    return Package{ .name = std::move(name->value), .version = std::move(version->value), .standard = *standard };
}

/**
 * Read the array of strings at @p name in @p table, if the table has one.
 */
std::expected<std::vector<StringField>, ManifestError> readStringArray(const std::filesystem::path& file,
                                                                       const toml::table& table,
                                                                       std::string_view prefix,
                                                                       std::string_view name)
{
    const toml::node* node = table.get(name);
    if (node == nullptr) {
        return std::vector<StringField>{};
    }
    const toml::array* items = node->as_array();
    if (items == nullptr) {
        return std::unexpected(errorAt(file, *node, dotted(prefix, name), "must be an array of strings"));
    }

    std::vector<StringField> fields;
    fields.reserve(items->size());
    for (const toml::node& item : *items) {
        if (! item.is_string()) {
            return std::unexpected(errorAt(file, item, dotted(prefix, name), "must be an array of strings"));
        }
        auto field = readString(file, item, dotted(prefix, name));
        if (! field.has_value()) {
            return std::unexpected(field.error());
        }
        fields.push_back(std::move(*field));
    }
    return fields;
}

/**
 * Read the array of strings at @p name in @p table into @p values.
 */
std::expected<void, ManifestError> readStrings(const std::filesystem::path& file,
                                               const toml::table& table,
                                               std::string_view prefix,
                                               std::string_view name,
                                               std::vector<std::string>& values)
{
    auto fields = readStringArray(file, table, prefix, name);
    if (! fields.has_value()) {
        return std::unexpected(fields.error());
    }
    for (StringField& field : *fields) {
        values.push_back(std::move(field.value));
    }
    return {};
}

/**
 * Read the settings @p table holds, reporting keys below @p prefix.
 */
std::expected<TargetSettings, ManifestError> parseSettings(const std::filesystem::path& file,
                                                           const toml::table& table,
                                                           std::string_view prefix)
{
    TargetSettings settings;

    auto includeDirectories = readStringArray(file, table, prefix, "include-dirs");
    if (! includeDirectories.has_value()) {
        return std::unexpected(includeDirectories.error());
    }
    for (const StringField& field : *includeDirectories) {
        if (auto valid = validateProjectPath(file, field, dotted(prefix, "include-dirs")); ! valid.has_value()) {
            return std::unexpected(valid.error());
        }
        settings.includeDirectories.emplace_back(field.value);
    }

    if (auto read = readStrings(file, table, prefix, "defines", settings.defines); ! read.has_value()) {
        return std::unexpected(read.error());
    }
    if (auto read = readStrings(file, table, prefix, "compile-flags", settings.compileFlags); ! read.has_value()) {
        return std::unexpected(read.error());
    }
    if (auto read = readStrings(file, table, prefix, "link-flags", settings.linkFlags); ! read.has_value()) {
        return std::unexpected(read.error());
    }
    return settings;
}

/**
 * Read the public table of a [[lib]] entry, if it has one.
 */
std::expected<TargetSettings, ManifestError> parsePublicSettings(const std::filesystem::path& file,
                                                                 const toml::table& entry,
                                                                 std::string_view key)
{
    const toml::node* node = entry.get("public");
    if (node == nullptr) {
        return TargetSettings{};
    }
    const std::string prefix = dotted(key, "public");
    const toml::table* table = node->as_table();
    if (table == nullptr) {
        return std::unexpected(errorAt(file, *node, prefix, "must be a table"));
    }
    if (auto known = rejectUnknownKeys(file, *table, prefix, KnownPublicKeys); ! known.has_value()) {
        return std::unexpected(known.error());
    }
    return parseSettings(file, *table, prefix);
}

/**
 * Reject a setting written where it has nowhere to go.
 *
 * Checked before unknown keys, so the report says why the key does not
 * belong there rather than that it is unknown.
 */
std::expected<void, ManifestError> rejectMisplacedSettings(const std::filesystem::path& file,
                                                           const toml::table& table,
                                                           std::string_view key,
                                                           TargetKind kind)
{
    if (kind == TargetKind::Executable) {
        if (const toml::node* node = table.get("public"); node != nullptr) {
            return std::unexpected(errorAt(file, *node, dotted(key, "public"), "nothing uses an executable; write these settings directly in [[bin]]"));
        }
        return {};
    }
    if (const toml::node* node = table.get("link-flags"); node != nullptr) {
        return std::unexpected(errorAt(file, *node, dotted(key, "link-flags"), "a static library is not linked; write it under lib.public"));
    }
    return {};
}

/**
 * The keys an entry of @p kind recognises.
 */
std::span<const std::string_view> knownTargetKeys(const TargetKind kind)
{
    switch (kind) {
    case TargetKind::Executable:
        return KnownExecutableKeys;
    case TargetKind::Library:
        return KnownLibraryKeys;
    }
    std::unreachable();
}

/**
 * Read the file src names, if the entry has one.
 *
 * A library is built from the sources below src/, so naming a file of its
 * own is optional; an executable is told apart by its entry point.
 */
std::expected<std::filesystem::path, ManifestError> parseSource(const std::filesystem::path& file,
                                                                const toml::table& table,
                                                                std::string_view key,
                                                                TargetKind kind)
{
    if ((kind == TargetKind::Library) && (table.get("src") == nullptr)) {
        return std::filesystem::path{};
    }
    auto source = requireString(file, table, key, "src");
    if (! source.has_value()) {
        return std::unexpected(source.error());
    }
    if (auto valid = validateProjectPath(file, *source, dotted(key, "src")); ! valid.has_value()) {
        return std::unexpected(valid.error());
    }
    return std::filesystem::path{ source->value };
}

/**
 * Parse one [[bin]] or [[lib]] entry and append it to @p targets.
 */
std::expected<void, ManifestError> parseTargetEntry(const std::filesystem::path& file,
                                                    const toml::table& table,
                                                    std::string_view key,
                                                    TargetKind kind,
                                                    std::vector<Target>& targets)
{
    if (auto placed = rejectMisplacedSettings(file, table, key, kind); ! placed.has_value()) {
        return std::unexpected(placed.error());
    }
    if (auto known = rejectUnknownKeys(file, table, key, knownTargetKeys(kind)); ! known.has_value()) {
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

    auto source = parseSource(file, table, key, kind);
    if (! source.has_value()) {
        return std::unexpected(source.error());
    }
    auto settings = parseSettings(file, table, key);
    if (! settings.has_value()) {
        return std::unexpected(settings.error());
    }
    auto publicSettings = parsePublicSettings(file, table, key);
    if (! publicSettings.has_value()) {
        return std::unexpected(publicSettings.error());
    }

    targets.push_back(Target{ .kind = kind, .name = std::move(name->value), .source = std::move(*source), .settings = std::move(*settings), .publicSettings = std::move(*publicSettings) });
    return {};
}

/**
 * Parse the array of tables named @p key, if the manifest has one.
 */
std::expected<void, ManifestError> parseTargetArray(const std::filesystem::path& file,
                                                    const toml::table& root,
                                                    std::string_view key,
                                                    TargetKind kind,
                                                    std::vector<Target>& targets)
{
    const toml::node* node = root.get(key);
    if (node == nullptr) {
        return {};
    }
    const toml::array* entries = node->as_array();
    if (entries == nullptr) {
        return std::unexpected(errorAt(file, *node, std::string{ key }, "must be an array of tables"));
    }

    for (const toml::node& entry : *entries) {
        const toml::table* table = entry.as_table();
        if (table == nullptr) {
            return std::unexpected(errorAt(file, entry, std::string{ key }, "must be an array of tables"));
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
std::expected<std::string, ManifestError> readWholeFile(const std::filesystem::path& file)
{
    std::error_code ec;
    if (! std::filesystem::is_regular_file(file, ec)) {
        return std::unexpected(unreadable(file, "cannot open the manifest"));
    }
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec) {
        return std::unexpected(unreadable(file, "cannot read the manifest"));
    }

    std::ifstream input(file, std::ios::binary);
    if (! input.is_open()) {
        return std::unexpected(unreadable(file, "cannot open the manifest"));
    }
    if (size == 0) {
        return std::string{};
    }

    std::string text(static_cast<std::size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if (static_cast<std::uintmax_t>(input.gcount()) != size) {
        return std::unexpected(unreadable(file, "cannot read the manifest"));
    }
    return text;
}

}  // anonymous namespace

/**
 * Parse manifest text into a Manifest.
 */
std::expected<Manifest, ManifestError> parseManifest(std::string_view text, const std::filesystem::path& file)
{
    const toml::parse_result parsed = toml::parse(text);
    if (! parsed) {
        const toml::parse_error& error = parsed.error();
        return std::unexpected(ManifestError{ .file = file, .position = toPosition(error.source().begin), .key = {}, .message = std::string{ error.description() }, .kind = ManifestErrorKind::Invalid });
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
    manifest.declaresTargets = ((parsed.table().get("bin") != nullptr) || (parsed.table().get("lib") != nullptr));

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
std::expected<Manifest, ManifestError> loadManifest(const std::filesystem::path& file)
{
    const auto text = readWholeFile(file);
    if (! text.has_value()) {
        return std::unexpected(text.error());
    }
    return parseManifest(*text, file);
}

}  // namespace scrap::Project
