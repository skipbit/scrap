#include "toolchain/GlobalConfig.h"

#include "toolchain/ToolchainChoice.h"
#include "toolchain/driver/TomlText.h"

#include <toml++/toml.hpp>

#include <expected>  // IWYU pragma: keep
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Toolchain {

namespace {

constexpr std::string_view DefaultsKey = "defaults";
constexpr std::string_view ToolchainKey = "toolchain";

/**
 * A fault at @p source, under @p key.
 */
GlobalConfigFault faultAt(const toml::source_region& source, std::string key, std::string message)
{
    return GlobalConfigFault{ .line = source.begin.line, .column = source.begin.column, .key = std::move(key), .message = std::move(message) };
}

std::expected<toml::table, GlobalConfigFault> parseConfig(std::string_view text)
{
    toml::parse_result parsed = toml::parse(text);
    if (! parsed) {
        const toml::parse_error& error = parsed.error();
        return std::unexpected(faultAt(error.source(), {}, std::string{ error.description() }));
    }
    return std::move(parsed).table();
}

/**
 * The default toolchain @p root holds, checked to be system or a specifier.
 */
std::expected<std::optional<std::string>, GlobalConfigFault> defaultIn(const toml::table& root)
{
    const toml::node* defaults = root.get(DefaultsKey);
    if (defaults == nullptr) {
        return std::nullopt;
    }
    const toml::table* table = defaults->as_table();
    if (table == nullptr) {
        return std::unexpected(faultAt(defaults->source(), "defaults", "must be a table"));
    }
    const toml::node* node = table->get(ToolchainKey);
    if (node == nullptr) {
        return std::nullopt;
    }
    std::optional<std::string> value = node->value_exact<std::string>();
    if (! value.has_value()) {
        return std::unexpected(faultAt(node->source(), "defaults.toolchain", "must be a string"));
    }
    if (auto choice = parseToolchainChoice(*value); ! choice.has_value()) {
        return std::unexpected(faultAt(node->source(), "defaults.toolchain", "is neither system nor a toolchain specifier: " + choice.error()));
    }
    return value;
}

}  // anonymous namespace

std::expected<std::optional<std::string>, GlobalConfigFault> readDefaultToolchain(std::string_view text)
{
    auto root = parseConfig(text);
    if (! root.has_value()) {
        return std::unexpected(root.error());
    }
    return defaultIn(*root);
}

std::expected<std::string, GlobalConfigFault> writeDefaultToolchain(std::string_view text, const std::optional<std::string>& toolchain)
{
    auto root = parseConfig(text);
    if (! root.has_value()) {
        return std::unexpected(root.error());
    }
    if (auto current = defaultIn(*root); ! current.has_value()) {
        return std::unexpected(current.error());
    }

    if (toolchain.has_value()) {
        if (! root->contains(DefaultsKey)) {
            root->insert(DefaultsKey, toml::table{});
        }
        root->get_as<toml::table>(DefaultsKey)->insert_or_assign(ToolchainKey, *toolchain);
    } else if (toml::table* defaults = root->get_as<toml::table>(DefaultsKey); defaults != nullptr) {
        defaults->erase(ToolchainKey);
        if (defaults->empty()) {
            root->erase(DefaultsKey);
        }
    }

    return tomlText(*root);
}

}  // namespace scrap::Toolchain
