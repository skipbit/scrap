#include "toolchain/ToolchainIndexParser.h"

#include "digest/DigestText.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainName.h"
#include "toolchain/ToolchainVersion.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Toolchain {

namespace {

/// The one format of the index this version reads.
constexpr std::int64_t SupportedSchemaVersion = 1;

/**
 * A fault at @p node, naming the line it starts on and its key.
 */
ToolchainIndexError faultAt(const toml::node& node, std::string_view key, std::string_view what)
{
    std::string message = "line " + std::to_string(node.source().begin.line) + ": ";
    message += key;
    message += ": ";
    message += what;
    return ToolchainIndexError{ .kind = ToolchainIndexErrorKind::Malformed, .message = std::move(message) };
}

/**
 * @p table's key joined below its own.
 */
std::string keyBelow(std::string_view table, std::string_view key)
{
    std::string joined{ table };
    joined += '.';
    joined += key;
    return joined;
}

bool isControlCharacter(char c)
{
    return static_cast<unsigned char>(c) < 0x20;
}

/**
 * The non-empty string @p table holds at @p key, free of control characters
 * so it cannot break a line it is later written into.
 */
std::expected<std::string, ToolchainIndexError> requireString(const toml::table& table, std::string_view tableKey, std::string_view key)
{
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return std::unexpected(faultAt(table, keyBelow(tableKey, key), "is missing"));
    }
    std::optional<std::string> value = node->value<std::string>();
    if (! value.has_value()) {
        return std::unexpected(faultAt(*node, keyBelow(tableKey, key), "must be a string"));
    }
    if (value->empty()) {
        return std::unexpected(faultAt(*node, keyBelow(tableKey, key), "must not be empty"));
    }
    if (std::ranges::any_of(*value, isControlCharacter)) {
        return std::unexpected(faultAt(*node, keyBelow(tableKey, key), "must not contain a control character"));
    }
    return std::move(*value);
}

/**
 * The name @p table holds at @p key, which must be present and able to stand
 * as one component of a path.
 */
std::expected<std::string, ToolchainIndexError> requireName(const toml::table& table, std::string_view tableKey, std::string_view key)
{
    auto name = requireString(table, tableKey, key);
    if (! name.has_value()) {
        return std::unexpected(name.error());
    }
    if (! isToolchainName(*name)) {
        return std::unexpected(faultAt(*table.get(key), keyBelow(tableKey, key), "must hold only lowercase letters, digits, '-', '_', '.' and '+', and not be '.' or '..'"));
    }
    return std::move(*name);
}

/**
 * The name @p table holds at @p key, or nothing when the key is absent.
 */
std::expected<std::optional<std::string>, ToolchainIndexError> optionalName(const toml::table& table, std::string_view tableKey, std::string_view key)
{
    if (! table.contains(key)) {
        return std::nullopt;
    }
    auto name = requireName(table, tableKey, key);
    if (! name.has_value()) {
        return std::unexpected(name.error());
    }
    return std::optional<std::string>{ std::move(*name) };
}

/**
 * The version @p table holds at @p key, which must be present.
 */
std::expected<ToolchainVersion, ToolchainIndexError> requireVersion(const toml::table& table, std::string_view tableKey, std::string_view key)
{
    auto text = requireString(table, tableKey, key);
    if (! text.has_value()) {
        return std::unexpected(text.error());
    }
    std::optional<ToolchainVersion> version = ToolchainVersion::parse(*text);
    if (! version.has_value()) {
        return std::unexpected(faultAt(*table.get(key), keyBelow(tableKey, key), "must be numbers joined by dots"));
    }
    return std::move(*version);
}

/**
 * The version @p table holds at @p key, or nothing when the key is absent.
 */
std::expected<std::optional<ToolchainVersion>, ToolchainIndexError> optionalVersion(const toml::table& table, std::string_view tableKey, std::string_view key)
{
    if (! table.contains(key)) {
        return std::nullopt;
    }
    auto version = requireVersion(table, tableKey, key);
    if (! version.has_value()) {
        return std::unexpected(version.error());
    }
    return std::optional<ToolchainVersion>{ std::move(*version) };
}

std::expected<RegistrySource, ToolchainIndexError> parseRegistrySource(const toml::table& ingot, std::string_view key)
{
    auto registry = requireString(ingot, key, "registry");
    if (! registry.has_value()) {
        return std::unexpected(registry.error());
    }
    auto tag = requireString(ingot, key, "tag");
    if (! tag.has_value()) {
        return std::unexpected(tag.error());
    }
    auto digest = requireString(ingot, key, "digest");
    if (! digest.has_value()) {
        return std::unexpected(digest.error());
    }
    if (! Digest::isSha256Digest(*digest)) {
        return std::unexpected(faultAt(*ingot.get("digest"), keyBelow(key, "digest"), "must be sha256: and 64 lowercase hex digits"));
    }
    return RegistrySource{ .registry = std::move(*registry), .tag = std::move(*tag), .digest = std::move(*digest) };
}

std::expected<UrlSource, ToolchainIndexError> parseUrlSource(const toml::table& ingot, std::string_view key)
{
    auto url = requireString(ingot, key, "url");
    if (! url.has_value()) {
        return std::unexpected(url.error());
    }
    auto sha256 = requireString(ingot, key, "sha256");
    if (! sha256.has_value()) {
        return std::unexpected(sha256.error());
    }
    if (! Digest::isSha256Hex(*sha256)) {
        return std::unexpected(faultAt(*ingot.get("sha256"), keyBelow(key, "sha256"), "must be 64 lowercase hex digits"));
    }
    return UrlSource{ .url = std::move(*url), .sha256 = std::move(*sha256) };
}

/**
 * Where @p ingot is fetched from, told by which keys it holds.
 */
std::expected<std::variant<RegistrySource, UrlSource>, ToolchainIndexError> parseSource(const toml::table& ingot, std::string_view key)
{
    const bool fromRegistry = ingot.contains("registry") || ingot.contains("tag") || ingot.contains("digest");
    const bool fromUrl = ingot.contains("url") || ingot.contains("sha256");
    if (fromRegistry && fromUrl) {
        return std::unexpected(faultAt(ingot, key, "names both a registry and a url"));
    }
    if (fromRegistry) {
        auto source = parseRegistrySource(ingot, key);
        if (! source.has_value()) {
            return std::unexpected(source.error());
        }
        return std::move(*source);
    }
    if (fromUrl) {
        auto source = parseUrlSource(ingot, key);
        if (! source.has_value()) {
            return std::unexpected(source.error());
        }
        return std::move(*source);
    }
    return std::unexpected(faultAt(ingot, key, "names neither a registry nor a url"));
}

std::expected<Ingot, ToolchainIndexError> parseIngot(const toml::table& ingot, std::string_view key)
{
    auto platform = requireName(ingot, key, "platform");
    if (! platform.has_value()) {
        return std::unexpected(platform.error());
    }
    auto arch = requireName(ingot, key, "arch");
    if (! arch.has_value()) {
        return std::unexpected(arch.error());
    }
    auto source = parseSource(ingot, key);
    if (! source.has_value()) {
        return std::unexpected(source.error());
    }
    auto glibcVersion = optionalVersion(ingot, key, "glibc_version");
    if (! glibcVersion.has_value()) {
        return std::unexpected(glibcVersion.error());
    }
    return Ingot{ .platform = std::move(*platform), .arch = std::move(*arch), .source = std::move(*source), .glibcVersion = std::move(*glibcVersion) };
}

std::expected<ToolchainStatus, ToolchainIndexError> parseStatus(const toml::table& toolchain, std::string_view key)
{
    auto status = requireString(toolchain, key, "status");
    if (! status.has_value()) {
        return std::unexpected(status.error());
    }
    if (*status == "active") {
        return ToolchainStatus::Active;
    }
    if (*status == "deprecated") {
        return ToolchainStatus::Deprecated;
    }
    if (*status == "disabled") {
        return ToolchainStatus::Disabled;
    }
    return std::unexpected(faultAt(*toolchain.get("status"), keyBelow(key, "status"), "must be active, deprecated or disabled"));
}

/**
 * The tables of the array of tables @p table holds at @p key, or none when
 * the key is absent.
 */
std::expected<std::vector<const toml::table*>, ToolchainIndexError> tablesAt(const toml::table& table, std::string_view tableKey, std::string_view key)
{
    std::vector<const toml::table*> tables;
    const toml::node* node = table.get(key);
    if (node == nullptr) {
        return tables;
    }
    const std::string fullKey = tableKey.empty() ? std::string{ key } : keyBelow(tableKey, key);
    const toml::array* array = node->as_array();
    if (array == nullptr) {
        return std::unexpected(faultAt(*node, fullKey, "must be an array of tables"));
    }
    for (const toml::node& item : *array) {
        const toml::table* entry = item.as_table();
        if (entry == nullptr) {
            return std::unexpected(faultAt(item, fullKey, "must be an array of tables"));
        }
        tables.push_back(entry);
    }
    return tables;
}

std::string indexed(std::string_view key, std::size_t index)
{
    return std::string{ key } + "[" + std::to_string(index) + "]";
}

std::expected<std::vector<Ingot>, ToolchainIndexError> parseIngots(const toml::table& toolchain, std::string_view key)
{
    auto tables = tablesAt(toolchain, key, "ingots");
    if (! tables.has_value()) {
        return std::unexpected(tables.error());
    }
    std::vector<Ingot> ingots;
    for (std::size_t i = 0; i < tables->size(); ++i) {
        const std::string ingotKey = indexed(keyBelow(key, "ingots"), i);
        auto ingot = parseIngot(*(*tables)[i], ingotKey);
        if (! ingot.has_value()) {
            return std::unexpected(ingot.error());
        }
        const bool repeated = std::ranges::any_of(ingots, [&](const Ingot& earlier) {
            return (earlier.platform == ingot->platform) && (earlier.arch == ingot->arch);
        });
        if (repeated) {
            return std::unexpected(faultAt(*(*tables)[i], ingotKey, "repeats an ingot for " + ingot->platform + "-" + ingot->arch));
        }
        ingots.push_back(std::move(*ingot));
    }
    return ingots;
}

/**
 * Read one toolchain. One that needs a newer scrap than @p scrapVersion is
 * kept with its family, version and the scrap it needs alone, so a shape
 * only that scrap knows does not stop this one from reading the index.
 */
std::expected<IndexedToolchain, ToolchainIndexError> parseToolchain(const toml::table& toolchain, std::string_view key, const ToolchainVersion& scrapVersion)
{
    auto family = requireName(toolchain, key, "family");
    if (! family.has_value()) {
        return std::unexpected(family.error());
    }
    auto version = requireVersion(toolchain, key, "version");
    if (! version.has_value()) {
        return std::unexpected(version.error());
    }
    auto minScrapVersion = optionalVersion(toolchain, key, "min_scrap_version");
    if (! minScrapVersion.has_value()) {
        return std::unexpected(minScrapVersion.error());
    }
    const std::optional<ToolchainVersion>& neededScrap = *minScrapVersion;
    if (neededScrap.has_value() && (*neededScrap > scrapVersion)) {
        return IndexedToolchain{
            .family = std::move(*family),
            .version = std::move(*version),
            .status = ToolchainStatus::Disabled,
            .compiler = {},
            .cCompiler = std::nullopt,
            .minScrapVersion = std::move(*minScrapVersion),
            .ingots = {},
        };
    }
    auto status = parseStatus(toolchain, key);
    if (! status.has_value()) {
        return std::unexpected(status.error());
    }
    auto compiler = requireName(toolchain, key, "compiler");
    if (! compiler.has_value()) {
        return std::unexpected(compiler.error());
    }
    auto cCompiler = optionalName(toolchain, key, "c_compiler");
    if (! cCompiler.has_value()) {
        return std::unexpected(cCompiler.error());
    }
    auto ingots = parseIngots(toolchain, key);
    if (! ingots.has_value()) {
        return std::unexpected(ingots.error());
    }
    return IndexedToolchain{
        .family = std::move(*family),
        .version = std::move(*version),
        .status = *status,
        .compiler = std::move(*compiler),
        .cCompiler = std::move(*cCompiler),
        .minScrapVersion = std::move(*minScrapVersion),
        .ingots = std::move(*ingots),
    };
}

/**
 * @p node as a reader would recognise it: a number or string as written,
 * anything else by its kind.
 */
std::string describeDeclared(const toml::node& node)
{
    if (const auto number = node.value_exact<std::int64_t>(); number.has_value()) {
        return std::to_string(*number);
    }
    if (const auto text = node.value_exact<std::string>(); text.has_value()) {
        return "\"" + *text + "\"";
    }
    switch (node.type()) {
    case toml::node_type::floating_point:
        return "a float";
    case toml::node_type::boolean:
        return "a boolean";
    case toml::node_type::table:
        return "a table";
    case toml::node_type::array:
        return "an array";
    default:
        return "a date or time";
    }
}

/**
 * Check that the index declares the one format this version reads.
 */
std::expected<void, ToolchainIndexError> checkFormat(const toml::table& root)
{
    const toml::node* header = root.get("index");
    if (header == nullptr) {
        return std::unexpected(faultAt(root, "index", "is missing"));
    }
    const toml::table* table = header->as_table();
    if (table == nullptr) {
        return std::unexpected(faultAt(*header, "index", "must be a table"));
    }
    const toml::node* schemaVersion = table->get("schema_version");
    if (schemaVersion == nullptr) {
        return std::unexpected(faultAt(*table, "index.schema_version", "is missing"));
    }
    if (schemaVersion->value_exact<std::int64_t>() != SupportedSchemaVersion) {
        return std::unexpected(ToolchainIndexError{ .kind = ToolchainIndexErrorKind::UnsupportedFormat, .message = describeDeclared(*schemaVersion) });
    }
    return {};
}

}  // anonymous namespace

std::expected<ToolchainIndex, ToolchainIndexError> parseToolchainIndex(std::string_view text, const ToolchainVersion& scrapVersion)
{
    const toml::parse_result parsed = toml::parse(text);
    if (! parsed) {
        const toml::parse_error& error = parsed.error();
        return std::unexpected(ToolchainIndexError{
            .kind = ToolchainIndexErrorKind::Malformed,
            .message = "line " + std::to_string(error.source().begin.line) + ": " + std::string{ error.description() },
        });
    }
    if (auto format = checkFormat(parsed.table()); ! format.has_value()) {
        return std::unexpected(format.error());
    }

    auto tables = tablesAt(parsed.table(), {}, "toolchains");
    if (! tables.has_value()) {
        return std::unexpected(tables.error());
    }
    ToolchainIndex index;
    for (std::size_t i = 0; i < tables->size(); ++i) {
        const std::string key = indexed("toolchains", i);
        auto toolchain = parseToolchain(*(*tables)[i], key, scrapVersion);
        if (! toolchain.has_value()) {
            return std::unexpected(toolchain.error());
        }
        const bool repeated = std::ranges::any_of(index.toolchains, [&](const IndexedToolchain& earlier) {
            return (earlier.family == toolchain->family) && (earlier.version == toolchain->version);
        });
        if (repeated) {
            return std::unexpected(faultAt(*(*tables)[i], key, "repeats " + toolchain->family + "@" + toolchain->version.text()));
        }
        index.toolchains.push_back(std::move(*toolchain));
    }
    return index;
}

}  // namespace scrap::Toolchain
