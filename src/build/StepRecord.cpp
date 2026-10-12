#include "build/StepRecord.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace scrap::Build {

namespace {

constexpr std::string_view Header = "scrap-step-record 1";
constexpr std::string_view ArgumentTag = "argument ";
constexpr std::string_view InputTag = "input ";
constexpr std::string_view EndTag = "end ";

/**
 * Append @p text to @p out with each backslash and line break escaped.
 */
void appendEscaped(std::string& out, const std::string_view text)
{
    for (const char ch : text) {
        if (ch == '\\') {
            out += "\\\\";
        } else if (ch == '\n') {
            out += "\\n";
        } else {
            out += ch;
        }
    }
}

/**
 * @p text with the escapes appendEscaped() writes undone, or nothing when it
 * holds any other.
 */
std::optional<std::string> unescape(const std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '\\') {
            out += text[index];
            continue;
        }
        if (++index == text.size()) {
            return std::nullopt;
        }
        if (text[index] == '\\') {
            out += '\\';
        } else if (text[index] == 'n') {
            out += '\n';
        } else {
            return std::nullopt;
        }
    }
    return out;
}

/**
 * The number @p field is as a whole, or nothing when it holds anything else.
 */
template <typename Number>
std::optional<Number> numberIn(const std::string_view field)
{
    Number number{};
    const auto [end, ec] = std::from_chars(field.data(), field.data() + field.size(), number);
    if ((ec != std::errc{}) || (end != field.data() + field.size())) {
        return std::nullopt;
    }
    return number;
}

/**
 * The part of @p text before its first space, with @p text moved past that
 * space, or nothing when @p text holds none.
 */
std::optional<std::string_view> takeField(std::string_view& text)
{
    const std::size_t space = text.find(' ');
    if (space == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view field = text.substr(0, space);
    text.remove_prefix(space + 1);
    return field;
}

/**
 * The input an "input" line names, from what follows the tag.
 */
std::optional<InputStamp> parseInput(std::string_view text)
{
    const auto modifiedField = takeField(text);
    if (! modifiedField.has_value()) {
        return std::nullopt;
    }
    const auto sizeField = takeField(text);
    if (! sizeField.has_value()) {
        return std::nullopt;
    }
    const auto modified = numberIn<std::int64_t>(*modifiedField);
    const auto size = numberIn<std::uintmax_t>(*sizeField);
    auto path = unescape(text);
    if (! (modified.has_value() && size.has_value() && path.has_value())) {
        return std::nullopt;
    }
    return InputStamp{ .path = std::move(*path), .modified = *modified, .size = *size };
}

/**
 * How many arguments and inputs the record says it holds.
 */
struct ItemCounts {
    std::size_t arguments;
    std::size_t inputs;
};

/**
 * The counts an "end" line gives, from what follows the tag.
 */
std::optional<ItemCounts> parseEnd(std::string_view text)
{
    const auto argumentsField = takeField(text);
    if (! argumentsField.has_value()) {
        return std::nullopt;
    }
    const auto arguments = numberIn<std::size_t>(*argumentsField);
    const auto inputs = numberIn<std::size_t>(text);
    if (! (arguments.has_value() && inputs.has_value())) {
        return std::nullopt;
    }
    return ItemCounts{ .arguments = *arguments, .inputs = *inputs };
}

}  // anonymous namespace

std::string renderStepRecord(const StepRecord& record)
{
    std::string out{ Header };
    out += '\n';
    for (const std::string& argument : record.arguments) {
        out += ArgumentTag;
        appendEscaped(out, argument);
        out += '\n';
    }
    for (const InputStamp& input : record.inputs) {
        out += InputTag;
        out += std::to_string(input.modified);
        out += ' ';
        out += std::to_string(input.size);
        out += ' ';
        appendEscaped(out, input.path.string());
        out += '\n';
    }
    out += EndTag;
    out += std::to_string(record.arguments.size());
    out += ' ';
    out += std::to_string(record.inputs.size());
    out += '\n';
    return out;
}

std::optional<StepRecord> parseStepRecord(std::string_view text)
{
    if (! text.ends_with('\n')) {
        return std::nullopt;
    }
    text.remove_suffix(1);
    std::vector<std::string_view> lines;
    for (std::size_t start = 0;;) {
        const std::size_t newline = text.find('\n', start);
        lines.push_back(text.substr(start, newline - start));
        if (newline == std::string_view::npos) {
            break;
        }
        start = newline + 1;
    }
    if ((lines.size() < 2) || (lines.front() != Header)) {
        return std::nullopt;
    }

    StepRecord record;
    std::size_t next = 1;
    for (; (next < lines.size()) && lines[next].starts_with(ArgumentTag); ++next) {
        auto argument = unescape(lines[next].substr(ArgumentTag.size()));
        if (! argument.has_value()) {
            return std::nullopt;
        }
        record.arguments.push_back(std::move(*argument));
    }
    for (; (next < lines.size()) && lines[next].starts_with(InputTag); ++next) {
        auto input = parseInput(lines[next].substr(InputTag.size()));
        if (! input.has_value()) {
            return std::nullopt;
        }
        record.inputs.push_back(std::move(*input));
    }
    if ((next + 1 != lines.size()) || ! lines[next].starts_with(EndTag)) {
        return std::nullopt;
    }
    const auto counts = parseEnd(lines[next].substr(EndTag.size()));
    if ((! counts.has_value()) || (counts->arguments != record.arguments.size()) || (counts->inputs != record.inputs.size())) {
        return std::nullopt;
    }
    return record;
}

}  // namespace scrap::Build
