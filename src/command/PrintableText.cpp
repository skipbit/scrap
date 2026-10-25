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
void appendEscaped(const unsigned char byte, std::string& text)
{
    static constexpr std::string_view HexDigits = "0123456789ABCDEF";
    text += "\\x";
    text += HexDigits[byte >> 4U];
    text += HexDigits[byte & 0x0FU];
}

/**
 * The length of the well-formed UTF-8 sequence starting at @p index, or 0 when
 * the bytes there do not form one.
 *
 * The ranges are those of the table of well-formed byte sequences in the
 * Unicode Standard, which rules out an encoding longer than it needs to be,
 * a surrogate and a value above U+10FFFF.
 */
std::size_t utf8SequenceLength(std::string_view text, const std::size_t index)
{
    const auto lead = static_cast<unsigned char>(text[index]);
    std::size_t length = 0;
    unsigned char secondLowest = 0x80U;
    unsigned char secondHighest = 0xBFU;
    if ((lead >= 0xC2U) && (lead <= 0xDFU)) {
        length = 2;
    } else if ((lead >= 0xE0U) && (lead <= 0xEFU)) {
        length = 3;
        if (lead == 0xE0U) {
            secondLowest = 0xA0U;
        } else if (lead == 0xEDU) {
            secondHighest = 0x9FU;
        }
    } else if ((lead >= 0xF0U) && (lead <= 0xF4U)) {
        length = 4;
        if (lead == 0xF0U) {
            secondLowest = 0x90U;
        } else if (lead == 0xF4U) {
            secondHighest = 0x8FU;
        }
    }
    if ((length == 0) || ((index + length) > text.size())) {
        return 0;
    }
    const auto second = static_cast<unsigned char>(text[index + 1]);
    if ((second < secondLowest) || (second > secondHighest)) {
        return 0;
    }
    for (std::size_t offset = 2; offset < length; ++offset) {
        if ((static_cast<unsigned char>(text[index + offset]) & 0xC0U) != 0x80U) {
            return 0;
        }
    }
    return length;
}

/**
 * The code point the well-formed sequence @p sequence encodes.
 */
char32_t codePointOf(const std::string_view sequence)
{
    char32_t codePoint = static_cast<unsigned char>(sequence.front()) & (0x7FU >> sequence.size());
    for (const char ch : sequence.substr(1)) {
        codePoint = (codePoint << 6U) | (static_cast<unsigned char>(ch) & 0x3FU);
    }
    return codePoint;
}

/**
 * Whether the well-formed character @p codePoint is escaped byte by byte: a
 * C1 control character, which a terminal in an eight-bit locale acts on, or
 * a character that changes the direction text is shown in, U+202A to U+202E
 * or U+2066 to U+2069.
 */
bool isEscapedCharacter(const char32_t codePoint)
{
    const bool isC1 = ((codePoint >= 0x80U) && (codePoint <= 0x9FU));
    const bool changesDirection = (((codePoint >= 0x202AU) && (codePoint <= 0x202EU)) || ((codePoint >= 0x2066U) && (codePoint <= 0x2069U)));
    return isC1 || changesDirection;
}

/**
 * Whether @p byte, an ASCII one, is laid out or quoted in a program's output
 * and kept there.
 */
bool keptInOutput(const unsigned char byte)
{
    return (byte == '\n') || (byte == '\t') || (byte == '\\');
}

/**
 * A byte outside ASCII is kept only as part of a well-formed UTF-8 sequence
 * that is neither a C1 control character nor a character that changes the
 * direction of text. A lone byte in that range is escaped, since a terminal
 * in an eight-bit locale acts on 0x80 to 0x9F as controls. The bytes
 * keptInOutput() names are kept too when @p isOutput says so.
 */
std::string escapeText(const std::string_view text, const bool isOutput)
{
    std::string result;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte < 0x80U) {
            const bool escaped = ((byte < 0x20U) || (byte == 0x7FU) || (text[index] == '\\'));
            if (escaped && ! (isOutput && keptInOutput(byte))) {
                appendEscaped(byte, result);
            } else {
                result += text[index];
            }
            ++index;
            continue;
        }

        const std::size_t length = utf8SequenceLength(text, index);
        if (length == 0) {
            appendEscaped(byte, result);
            ++index;
        } else if (isEscapedCharacter(codePointOf(text.substr(index, length)))) {
            for (std::size_t offset = 0; offset < length; ++offset) {
                appendEscaped(static_cast<unsigned char>(text[index + offset]), result);
            }
            index += length;
        } else {
            result.append(text, index, length);
            index += length;
        }
    }
    return result;
}

}  // anonymous namespace

std::string printableText(const std::string_view text)
{
    return escapeText(text, false);
}

std::string printableLines(const std::string_view text)
{
    return escapeText(text, true);
}

std::string printablePath(const std::filesystem::path& path)
{
    return printableText(path.string());
}

std::string printableName(const std::string_view name)
{
    std::string result;
    for (const char ch : name) {
        const auto byte = static_cast<unsigned char>(ch);
        if ((byte >= 0x20U) && (byte < 0x7FU) && (ch != '\\')) {
            result += ch;
        } else {
            appendEscaped(byte, result);
        }
    }
    return result;
}

}  // namespace scrap::Command
