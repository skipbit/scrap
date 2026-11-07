#include "json/JsonValue.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace scrap::Json {

const Value* Value::member(const std::string_view name) const
{
    const auto* object = std::get_if<Object>(&data);
    if (object == nullptr) {
        return nullptr;
    }
    for (const Member& candidate : *object) {
        if (candidate.name == name) {
            return &candidate.value;
        }
    }
    return nullptr;
}

const std::string* Value::stringMember(const std::string_view name) const
{
    const Value* found = member(name);
    return (found == nullptr) ? nullptr : found->asString();
}

const std::string* Value::asString() const
{
    return std::get_if<std::string>(&data);
}

const Array* Value::asArray() const
{
    return std::get_if<Array>(&data);
}

std::optional<std::uint64_t> Value::asUnsigned() const
{
    const auto* number = std::get_if<Number>(&data);
    if (number == nullptr) {
        return std::nullopt;
    }
    const std::string& text = number->text;
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if ((error != std::errc{}) || (end != text.data() + text.size())) {
        return std::nullopt;
    }
    return result;
}

}  // namespace scrap::Json
