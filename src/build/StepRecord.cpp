#include "build/StepRecord.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
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
 * The number at the start of @p text, written as renderStepRecord() writes
 * it and followed by @p separator, with @p text moved past both.
 */
template <typename Number>
std::optional<Number> takeNumber(std::string_view& text, const std::string_view separator)
{
    Number number{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (ec != std::errc{}) {
        return std::nullopt;
    }
    text.remove_prefix(static_cast<std::size_t>(end - text.data()));
    if (! text.starts_with(separator)) {
        return std::nullopt;
    }
    text.remove_prefix(separator.size());
    return number;
}

/**
 * The input an "input" line names, from what follows the tag.
 */
std::optional<InputStamp> parseInput(std::string_view text)
{
    const auto modified = takeNumber<std::int64_t>(text, " ");
    if (! modified.has_value()) {
        return std::nullopt;
    }
    const auto size = takeNumber<std::uintmax_t>(text, " ");
    if (! size.has_value()) {
        return std::nullopt;
    }
    auto path = unescape(text);
    if (! path.has_value()) {
        return std::nullopt;
    }
    return InputStamp{ .path = std::move(*path), .modified = *modified, .size = *size };
}

/**
 * Whether @p text is the line that ends @p record.
 */
bool endsRecord(std::string_view text, const StepRecord& record)
{
    const auto arguments = takeNumber<std::size_t>(text, " ");
    if (! arguments.has_value()) {
        return false;
    }
    std::size_t inputs = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), inputs);
    return (ec == std::errc{}) && (end == text.data() + text.size()) && (*arguments == record.arguments.size()) && (inputs == record.inputs.size());
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
    for (const std::string_view line : std::span{ lines }.subspan(1, lines.size() - 2)) {
        if (line.starts_with(ArgumentTag) && record.inputs.empty()) {
            auto argument = unescape(line.substr(ArgumentTag.size()));
            if (! argument.has_value()) {
                return std::nullopt;
            }
            record.arguments.push_back(std::move(*argument));
        } else if (line.starts_with(InputTag)) {
            auto input = parseInput(line.substr(InputTag.size()));
            if (! input.has_value()) {
                return std::nullopt;
            }
            record.inputs.push_back(std::move(*input));
        } else {
            return std::nullopt;
        }
    }
    if (! (lines.back().starts_with(EndTag) && endsRecord(lines.back().substr(EndTag.size()), record))) {
        return std::nullopt;
    }
    return record;
}

}  // namespace scrap::Build
