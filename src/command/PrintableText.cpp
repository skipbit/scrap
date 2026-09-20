#include "command/PrintableText.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace scrap::Command {

namespace {

/**
 * Append @p byte to @p text as \xNN.
 */
auto appendEscaped(const unsigned char byte, std::string& text) -> void
{
    static constexpr std::string_view HexDigits = "0123456789ABCDEF";
    text += "\\x";
    text += HexDigits[byte >> 4U];
    text += HexDigits[byte & 0x0FU];
}

/**
 * The length of the well-formed UTF-8 sequence starting at @p index, or 0 when
 * the bytes there do not form one.
 */
auto utf8SequenceLength(std::string_view text, const std::size_t index) -> std::size_t
{
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t length = 0;
    if ((lead & 0xE0U) == 0xC0U) {
        length = 2;
    } else if ((lead & 0xF0U) == 0xE0U) {
        length = 3;
    } else if ((lead & 0xF8U) == 0xF0U) {
        length = 4;
    }
    if (length == 0 || index + length > text.size()) {
        return 0;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        if ((static_cast<unsigned char>(text[index + offset]) & 0xC0U) != 0x80U) {
            return 0;
        }
    }
    return length;
}

}  // anonymous namespace

/**
 * A byte outside ASCII is kept only as part of a well-formed UTF-8 sequence
 * that is not a C1 control character. A lone byte in that range is escaped,
 * since a terminal in an eight-bit locale acts on 0x80 to 0x9F as controls.
 */
auto printableText(const std::string_view text) -> std::string
{
    std::string result;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte < 0x80U) {
            if (byte < 0x20U || byte == 0x7FU || text[index] == '\\') {
                appendEscaped(byte, result);
            } else {
                result += text[index];
            }
            ++index;
            continue;
        }

        const std::size_t length = utf8SequenceLength(text, index);
        const bool isC1 = length == 2 && byte == 0xC2U && static_cast<unsigned char>(text[index + 1]) <= 0x9FU;
        if (length == 0) {
            appendEscaped(byte, result);
            ++index;
        } else if (isC1) {
            appendEscaped(byte, result);
            appendEscaped(static_cast<unsigned char>(text[index + 1]), result);
            index += 2;
        } else {
            result.append(text, index, length);
            index += length;
        }
    }
    return result;
}

auto printablePath(const std::filesystem::path& path) -> std::string
{
    return printableText(path.string());
}

auto printableName(const std::string_view name) -> std::string
{
    std::string result;
    for (const char ch : name) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte >= 0x20U && byte < 0x7FU && ch != '\\') {
            result += ch;
        } else {
            appendEscaped(byte, result);
        }
    }
    return result;
}

}  // namespace scrap::Command
