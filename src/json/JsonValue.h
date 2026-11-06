#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace scrap::Json {

struct Value;
struct Member;

/**
 * @brief JSON null.
 */
struct Null {
    bool operator==(const Null&) const = default;
};

/**
 * @brief A JSON number, kept as written so no digit is lost.
 */
struct Number {
    std::string text;
    bool operator==(const Number&) const = default;
};

using Array = std::vector<Value>;

/// The members of an object, in the order written. No two share a name.
using Object = std::vector<Member>;

/**
 * @brief A JSON value.
 */
struct Value {
    std::variant<Null, bool, Number, std::string, Array, Object> data;

    /// The member named @p name, when this is an object that has one.
    [[nodiscard]] const Value* member(std::string_view name) const;

    [[nodiscard]] const std::string* asString() const;
    [[nodiscard]] const Array* asArray() const;

    /// The number, when this is one written as a whole number that fits.
    [[nodiscard]] std::optional<std::uint64_t> asUnsigned() const;

    bool operator==(const Value&) const = default;
};

/**
 * @brief One member of a JSON object.
 */
struct Member {
    std::string name;
    Value value;
    bool operator==(const Member&) const = default;
};

}  // namespace scrap::Json
