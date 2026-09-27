#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Command {

enum class OptionValueType : std::uint8_t {
    Bool,
    Int64,
    String,
    StringList
};

using OptionValue = std::variant<bool, std::int64_t, std::string, std::vector<std::string>>;

struct OptionDef {
    std::string longName;
    std::optional<char> shortName;
    OptionValueType type;
    bool required = false;
    std::string description;
    std::optional<OptionValue> defaultValue;
    std::vector<std::string> choices;
};

struct PositionalDef {
    std::string name;
    std::string description;
    bool required = true;
};

/**
 * The arguments a command takes after "--", passed on as written.
 */
struct TrailingDef {
    std::string name;
    std::string description;
};

struct OptionSchema {
    std::vector<OptionDef> named;
    std::vector<PositionalDef> positional;
    std::optional<TrailingDef> trailing;  ///< Set when the command takes arguments after "--".
};

}  // namespace scrap::Command
