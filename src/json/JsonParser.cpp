#include "json/JsonParser.h"

#include "json/JsonValue.h"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Json {

namespace {

constexpr char32_t FirstHighSurrogate = 0xD800;
constexpr char32_t FirstLowSurrogate = 0xDC00;
constexpr char32_t PastLastSurrogate = 0xE000;
constexpr char32_t SurrogateBits = 10;
constexpr char32_t SurrogateMask = 0x3FF;
constexpr char32_t FirstSupplementary = 0x10000;
constexpr int HexDigitsInEscape = 4;
constexpr unsigned BitsPerHexDigit = 4;
constexpr unsigned char FirstPrintable = 0x20;

constexpr char32_t PastOneByte = 0x80;
constexpr char32_t PastTwoBytes = 0x800;
constexpr unsigned ContinuationShift = 6;
constexpr unsigned ContinuationMask = 0x3F;
constexpr unsigned ContinuationMark = 0x80;
constexpr unsigned TwoByteMark = 0xC0;
constexpr unsigned ThreeByteMark = 0xE0;
constexpr unsigned FourByteMark = 0xF0;

void appendUtf8(std::string& out, const char32_t point)
{
    const auto byte = [&out](const unsigned value) {
        out.push_back(static_cast<char>(value));
    };
    const auto continuation = [&byte](const char32_t value, const unsigned shift) {
        byte(ContinuationMark | ((value >> shift) & ContinuationMask));
    };
    if (point < PastOneByte) {
        byte(point);
    } else if (point < PastTwoBytes) {
        byte(TwoByteMark | (point >> ContinuationShift));
        continuation(point, 0);
    } else if (point < FirstSupplementary) {
        byte(ThreeByteMark | (point >> (2 * ContinuationShift)));
        continuation(point, ContinuationShift);
        continuation(point, 0);
    } else {
        byte(FourByteMark | (point >> (3 * ContinuationShift)));
        continuation(point, 2 * ContinuationShift);
        continuation(point, ContinuationShift);
        continuation(point, 0);
    }
}

std::optional<char32_t> hexValue(const char c)
{
    if ((c >= '0') && (c <= '9')) {
        return static_cast<char32_t>(c - '0');
    }
    if ((c >= 'a') && (c <= 'f')) {
        return static_cast<char32_t>(c - 'a' + 10);
    }
    if ((c >= 'A') && (c <= 'F')) {
        return static_cast<char32_t>(c - 'A' + 10);
    }
    return std::nullopt;
}

bool isDigit(const char c)
{
    return (c >= '0') && (c <= '9');
}

/**
 * An array or object whose closing bracket has not been read yet.
 */
struct OpenContainer {
    Value container;
    std::string pendingName;  ///< The name of the member being read, in an object.
};

/**
 * Reads one value from the text, keeping where it is.
 */
class Reader {
public:
    explicit Reader(const std::string_view text)
        : _text(text)
    {
    }

    /**
     * Read the whole text as one value.
     *
     * Arrays and objects being read wait on a stack of their own rather than
     * on the call stack, so how deeply they nest is checked against the limit
     * alone.
     */
    std::expected<Value, ParseError> document()
    {
        std::vector<OpenContainer> open;
        while (true) {
            auto finished = readNext(open);
            if (! finished.has_value()) {
                return std::unexpected(std::move(finished.error()));
            }
            if (! finished->has_value()) {
                continue;
            }
            auto placed = place(open, std::move(**finished));
            if (! placed.has_value()) {
                return std::unexpected(std::move(placed.error()));
            }
            if (placed->has_value()) {
                return wholeText(std::move(**placed));
            }
        }
    }

private:
    [[nodiscard]] std::unexpected<ParseError> fail(std::string reason) const
    {
        return std::unexpected(ParseError{ .offset = _at, .reason = std::move(reason) });
    }

    [[nodiscard]] bool atEnd() const
    {
        return _at >= _text.size();
    }

    [[nodiscard]] char peek() const
    {
        return atEnd() ? '\0' : _text[_at];
    }

    bool consume(const char expected)
    {
        if (atEnd() || (_text[_at] != expected)) {
            return false;
        }
        ++_at;
        return true;
    }

    void skipWhitespace()
    {
        while ((! atEnd()) && ((peek() == ' ') || (peek() == '\t') || (peek() == '\n') || (peek() == '\r'))) {
            ++_at;
        }
    }

    std::expected<Value, ParseError> readScalar()
    {
        switch (peek()) {
        case '"': {
            auto text = readString();
            if (! text.has_value()) {
                return std::unexpected(std::move(text.error()));
            }
            return Value{ std::move(*text) };
        }
        case 't':
            return readLiteral("true", Value{ true });
        case 'f':
            return readLiteral("false", Value{ false });
        case 'n':
            return readLiteral("null", Value{ Null{} });
        default:
            return readNumber();
        }
    }

    std::expected<Value, ParseError> readLiteral(const std::string_view word, Value value)
    {
        if (! _text.substr(_at).starts_with(word)) {
            return fail("expected a value");
        }
        _at += word.size();
        return value;
    }

    std::expected<Value, ParseError> readNumber()
    {
        const std::size_t start = _at;
        consume('-');
        if (consume('0')) {
            // A leading zero stands alone.
        } else if (isDigit(peek())) {
            skipDigits();
        } else {
            return fail("expected a value");
        }
        if (consume('.')) {
            if (! isDigit(peek())) {
                return fail("expected a digit after the decimal point");
            }
            skipDigits();
        }
        if (consume('e') || consume('E')) {
            if (! consume('+')) {
                consume('-');
            }
            if (! isDigit(peek())) {
                return fail("expected a digit in the exponent");
            }
            skipDigits();
        }
        return Value{ Number{ std::string{ _text.substr(start, _at - start) } } };
    }

    void skipDigits()
    {
        while (isDigit(peek())) {
            ++_at;
        }
    }

    std::expected<std::string, ParseError> readString()
    {
        ++_at;  // the opening quote
        std::string out;
        while (true) {
            if (atEnd()) {
                return fail("unterminated string");
            }
            const char c = _text[_at];
            if (c == '"') {
                ++_at;
                return out;
            }
            if (static_cast<unsigned char>(c) < FirstPrintable) {
                return fail("control character in a string");
            }
            if (c != '\\') {
                out.push_back(c);
                ++_at;
                continue;
            }
            ++_at;
            if (auto escaped = readEscape(out); ! escaped.has_value()) {
                return std::unexpected(std::move(escaped.error()));
            }
        }
    }

    std::expected<void, ParseError> readEscape(std::string& out)
    {
        if (atEnd()) {
            return fail("unterminated string");
        }
        const char c = _text[_at++];
        switch (c) {
        case '"':
        case '\\':
        case '/':
            out.push_back(c);
            return {};
        case 'b':
            out.push_back('\b');
            return {};
        case 'f':
            out.push_back('\f');
            return {};
        case 'n':
            out.push_back('\n');
            return {};
        case 'r':
            out.push_back('\r');
            return {};
        case 't':
            out.push_back('\t');
            return {};
        case 'u':
            return readUnicodeEscape(out);
        default:
            --_at;
            return fail("unknown escape in a string");
        }
    }

    std::expected<char32_t, ParseError> readHexUnit()
    {
        char32_t unit = 0;
        for (int digit = 0; digit < HexDigitsInEscape; ++digit) {
            const std::optional<char32_t> value = atEnd() ? std::nullopt : hexValue(_text[_at]);
            if (! value.has_value()) {
                return fail("expected four hex digits after \\u");
            }
            unit = (unit << BitsPerHexDigit) | *value;
            ++_at;
        }
        return unit;
    }

    std::expected<void, ParseError> readUnicodeEscape(std::string& out)
    {
        auto unit = readHexUnit();
        if (! unit.has_value()) {
            return std::unexpected(std::move(unit.error()));
        }
        if ((*unit >= FirstLowSurrogate) && (*unit < PastLastSurrogate)) {
            return fail("a low surrogate without a high one");
        }
        if ((*unit < FirstHighSurrogate) || (*unit >= FirstLowSurrogate)) {
            appendUtf8(out, *unit);
            return {};
        }
        if ((! consume('\\')) || (! consume('u'))) {
            return fail("a high surrogate without a low one");
        }
        auto low = readHexUnit();
        if (! low.has_value()) {
            return std::unexpected(std::move(low.error()));
        }
        if ((*low < FirstLowSurrogate) || (*low >= PastLastSurrogate)) {
            return fail("a high surrogate without a low one");
        }
        appendUtf8(out, FirstSupplementary + (((*unit - FirstHighSurrogate) << SurrogateBits) | (*low & SurrogateMask)));
        return {};
    }

    /**
     * Read the name of the next member of @p object, and the colon after it.
     */
    std::expected<void, ParseError> readMemberName(OpenContainer& object)
    {
        skipWhitespace();
        if (peek() != '"') {
            return fail("expected a member name");
        }
        const std::size_t nameAt = _at;
        auto name = readString();
        if (! name.has_value()) {
            return std::unexpected(std::move(name.error()));
        }
        if (object.container.member(*name) != nullptr) {
            _at = nameAt;
            return fail("the member '" + *name + "' appears twice");
        }
        skipWhitespace();
        if (! consume(':')) {
            return fail("expected ':'");
        }
        object.pendingName = std::move(*name);
        return {};
    }

    /**
     * Read the next value, or the start of an array or object.
     *
     * @return The value once it is whole, or nothing when an array or object
     *         was opened and waits for its members.
     */
    std::expected<std::optional<Value>, ParseError> readNext(std::vector<OpenContainer>& open)
    {
        skipWhitespace();
        if ((peek() == '{') || (peek() == '[')) {
            return openContainer(open);
        }
        auto scalar = readScalar();
        if (! scalar.has_value()) {
            return std::unexpected(std::move(scalar.error()));
        }
        return std::move(*scalar);
    }

    std::expected<std::optional<Value>, ParseError> openContainer(std::vector<OpenContainer>& open)
    {
        if (open.size() >= NestingLimit) {
            return fail("nested too deeply");
        }
        const bool isObject = (peek() == '{');
        ++_at;
        open.push_back(OpenContainer{ .container = isObject ? Value{ Object{} } : Value{ Array{} }, .pendingName = {} });
        skipWhitespace();
        if (consume(isObject ? '}' : ']')) {
            Value empty = std::move(open.back().container);
            open.pop_back();
            return empty;
        }
        if (isObject) {
            if (auto named = readMemberName(open.back()); ! named.has_value()) {
                return std::unexpected(std::move(named.error()));
            }
        }
        return std::nullopt;
    }

    /**
     * Put @p value into the innermost open container, and close each container
     * that ends after it.
     *
     * @return The whole value once nothing is left open, or nothing while
     *         something is.
     */
    std::expected<std::optional<Value>, ParseError> place(std::vector<OpenContainer>& open, Value value)
    {
        while (! open.empty()) {
            OpenContainer& innermost = open.back();
            addTo(innermost, std::move(value));
            auto closed = readAfterMember(innermost);
            if (! closed.has_value()) {
                return std::unexpected(std::move(closed.error()));
            }
            if (! *closed) {
                return std::nullopt;
            }
            value = std::move(innermost.container);
            open.pop_back();
        }
        return value;
    }

    static void addTo(OpenContainer& open, Value value)
    {
        if (auto* object = std::get_if<Object>(&open.container.data)) {
            object->push_back(Member{ .name = std::move(open.pendingName), .value = std::move(value) });
        } else {
            std::get<Array>(open.container.data).push_back(std::move(value));
        }
    }

    /**
     * Read what follows a member of @p open: its end, or a comma and, in an
     * object, the name of the next member.
     *
     * @return Whether @p open has ended.
     */
    std::expected<bool, ParseError> readAfterMember(OpenContainer& open)
    {
        const bool isObject = std::holds_alternative<Object>(open.container.data);
        skipWhitespace();
        if (consume(isObject ? '}' : ']')) {
            return true;
        }
        if (! consume(',')) {
            return fail(isObject ? "expected ',' or '}'" : "expected ',' or ']'");
        }
        if (isObject) {
            if (auto named = readMemberName(open); ! named.has_value()) {
                return std::unexpected(std::move(named.error()));
            }
        }
        return false;
    }

    std::expected<Value, ParseError> wholeText(Value value)
    {
        skipWhitespace();
        if (_at != _text.size()) {
            return fail("unexpected text after the value");
        }
        return value;
    }

    std::string_view _text;
    std::size_t _at = 0;
};

}  // anonymous namespace

std::expected<Value, ParseError> parse(const std::string_view text)
{
    return Reader{ text }.document();
}

}  // namespace scrap::Json
