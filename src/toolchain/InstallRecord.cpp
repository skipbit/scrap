#include "toolchain/InstallRecord.h"

#include "toolchain/driver/TomlText.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Toolchain {

namespace {

constexpr std::string_view CompilerKey = "compiler";
constexpr std::string_view RuntimeLibraryDirectoriesKey = "runtime-library-dirs";
constexpr std::string_view CompileArgumentsKey = "compile-args";
constexpr std::string_view LinkArgumentsKey = "link-args";
constexpr std::string_view IngotIntegrityKey = "ingot-integrity";
constexpr std::string_view CCompilerKey = "c-compiler";
constexpr std::string_view SourceKey = "source";
constexpr std::string_view InstalledAtKey = "installed-at";

/**
 * Whether @p path names something inside the directory it is taken against:
 * relative, and with no component that leads up out of it.
 */
bool staysInside(const std::filesystem::path& path)
{
    if (path.empty() || path.has_root_path()) {
        return false;
    }
    return std::ranges::none_of(path, [](const std::filesystem::path& component) {
        return component == "..";
    });
}

/**
 * Why @p node, the value of @p key, is refused, starting with its line.
 */
std::string fault(const toml::node& node, std::string_view key, std::string_view reason)
{
    std::string message = "line " + std::to_string(node.source().begin.line) + ": ";
    message += key;
    message += ": ";
    message += reason;
    return message;
}

/**
 * @p node, the value of @p key, as a path inside the toolchain.
 */
std::expected<std::filesystem::path, std::string> pathInside(const toml::node& node, std::string_view key)
{
    const std::optional<std::string> text = node.value_exact<std::string>();
    if (! text.has_value()) {
        return std::unexpected(fault(node, key, "must be a string"));
    }
    if (! staysInside(*text)) {
        return std::unexpected(fault(node, key, "must be a relative path inside the toolchain"));
    }
    return std::filesystem::path{ *text };
}

/**
 * The runtime library directories @p table records, none when it records
 * no list of them.
 */
std::expected<std::vector<std::filesystem::path>, std::string> runtimeLibraryDirectories(const toml::table& table)
{
    const toml::node* node = table.get(RuntimeLibraryDirectoriesKey);
    if (node == nullptr) {
        return std::vector<std::filesystem::path>{};
    }
    const toml::array* entries = node->as_array();
    if (entries == nullptr) {
        return std::unexpected(fault(*node, RuntimeLibraryDirectoriesKey, "must be an array of strings"));
    }
    std::vector<std::filesystem::path> directories;
    for (const toml::node& entry : *entries) {
        auto directory = pathInside(entry, RuntimeLibraryDirectoriesKey);
        if (! directory.has_value()) {
            return std::unexpected(std::move(directory.error()));
        }
        directories.push_back(std::move(*directory));
    }
    return directories;
}

/**
 * The arguments @p table records under @p key, none when it records no list
 * of them.
 */
std::expected<std::vector<std::string>, std::string> arguments(const toml::table& table, std::string_view key)
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::vector<std::string>{};
    }
    const toml::array* entries = node->as_array();
    if (entries == nullptr) {
        return std::unexpected(fault(*node, key, "must be an array of strings"));
    }
    std::vector<std::string> found;
    for (const toml::node& entry : *entries) {
        std::optional<std::string> text = entry.value_exact<std::string>();
        if (! text.has_value()) {
            return std::unexpected(fault(entry, key, "must be a string"));
        }
        found.push_back(std::move(*text));
    }
    return found;
}

/**
 * The string @p table records under @p key, nothing when it records none.
 */
std::expected<std::optional<std::string>, std::string> optionalString(const toml::table& table, std::string_view key)
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    std::optional<std::string> text = node->value_exact<std::string>();
    if (! text.has_value()) {
        return std::unexpected(fault(*node, key, "must be a string"));
    }
    return text;
}

/**
 * The path inside the toolchain @p table records under @p key, nothing when
 * it records none.
 */
std::expected<std::optional<std::filesystem::path>, std::string> optionalPathInside(const toml::table& table, std::string_view key)
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    auto path = pathInside(*node, key);
    if (! path.has_value()) {
        return std::unexpected(std::move(path.error()));
    }
    return std::optional<std::filesystem::path>{ std::move(*path) };
}

/**
 * Put @p value into @p table under @p key, when there is one.
 */
void insertIfKnown(toml::table& table, std::string_view key, const std::optional<std::string>& value)
{
    if (value.has_value()) {
        table.insert(key, *value);
    }
}

/**
 * Put @p path into @p table under @p key, when there is one.
 */
void insertIfKnown(toml::table& table, std::string_view key, const std::optional<std::filesystem::path>& path)
{
    insertIfKnown(table, key, path.transform([](const std::filesystem::path& known) {
        return known.generic_string();
    }));
}

/**
 * Put @p list into @p table under @p key, unless it is empty.
 */
void insertArguments(toml::table& table, std::string_view key, const std::vector<std::string>& list)
{
    if (list.empty()) {
        return;
    }
    toml::array entries;
    for (const std::string& argument : list) {
        entries.push_back(argument);
    }
    table.insert(key, std::move(entries));
}

}  // anonymous namespace

std::expected<InstallRecord, std::string> parseInstallRecord(std::string_view text)
{
    const toml::parse_result parsed = toml::parse(text);
    if (! parsed) {
        const toml::parse_error& error = parsed.error();
        return std::unexpected("line " + std::to_string(error.source().begin.line) + ": " + std::string{ error.description() });
    }
    const toml::node* node = parsed.table().get(CompilerKey);
    if (node == nullptr) {
        return std::unexpected(std::string{ CompilerKey } + ": is missing");
    }
    auto compiler = pathInside(*node, CompilerKey);
    if (! compiler.has_value()) {
        return std::unexpected(std::move(compiler.error()));
    }
    auto directories = runtimeLibraryDirectories(parsed.table());
    if (! directories.has_value()) {
        return std::unexpected(std::move(directories.error()));
    }
    auto compileArguments = arguments(parsed.table(), CompileArgumentsKey);
    if (! compileArguments.has_value()) {
        return std::unexpected(std::move(compileArguments.error()));
    }
    auto linkArguments = arguments(parsed.table(), LinkArgumentsKey);
    if (! linkArguments.has_value()) {
        return std::unexpected(std::move(linkArguments.error()));
    }
    auto integrity = optionalString(parsed.table(), IngotIntegrityKey);
    if (! integrity.has_value()) {
        return std::unexpected(std::move(integrity.error()));
    }
    auto cCompiler = optionalPathInside(parsed.table(), CCompilerKey);
    if (! cCompiler.has_value()) {
        return std::unexpected(std::move(cCompiler.error()));
    }
    auto source = optionalString(parsed.table(), SourceKey);
    if (! source.has_value()) {
        return std::unexpected(std::move(source.error()));
    }
    auto installedAt = optionalString(parsed.table(), InstalledAtKey);
    if (! installedAt.has_value()) {
        return std::unexpected(std::move(installedAt.error()));
    }
    return InstallRecord{ .compiler = std::move(*compiler),
                          .runtimeLibraryDirectories = std::move(*directories),
                          .arguments = { .compile = std::move(*compileArguments), .link = std::move(*linkArguments) },
                          .ingotIntegrity = std::move(*integrity),
                          .cCompiler = std::move(*cCompiler),
                          .source = std::move(*source),
                          .installedAt = std::move(*installedAt) };
}

std::string renderInstallRecord(const InstallRecord& record)
{
    toml::table table;
    table.insert(CompilerKey, record.compiler.generic_string());
    toml::array directories;
    for (const std::filesystem::path& directory : record.runtimeLibraryDirectories) {
        directories.push_back(directory.generic_string());
    }
    table.insert(RuntimeLibraryDirectoriesKey, std::move(directories));
    insertArguments(table, CompileArgumentsKey, record.arguments.compile);
    insertArguments(table, LinkArgumentsKey, record.arguments.link);
    insertIfKnown(table, IngotIntegrityKey, record.ingotIntegrity);
    insertIfKnown(table, CCompilerKey, record.cCompiler);
    insertIfKnown(table, SourceKey, record.source);
    insertIfKnown(table, InstalledAtKey, record.installedAt);
    return tomlText(table);
}

}  // namespace scrap::Toolchain
