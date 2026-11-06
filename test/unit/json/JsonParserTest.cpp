#include "json/JsonParser.h"

#include "json/JsonValue.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

using scrap::Json::Array;
using scrap::Json::Member;
using scrap::Json::NestingLimit;
using scrap::Json::Null;
using scrap::Json::Number;
using scrap::Json::Object;
using scrap::Json::parse;
using scrap::Json::Value;

namespace {

/// The JSON escape of the code unit @p hex.
std::string escape(const std::string_view hex)
{
    return std::string{ '\\', 'u' } + std::string{ hex };
}

std::string quoted(const std::string& text)
{
    return "\"" + text + "\"";
}

/// The reason @p text is refused, or a note that it was read.
std::string reasonFor(const std::string_view text)
{
    const auto parsed = parse(text);
    return parsed.has_value() ? std::string{ "(read)" } : parsed.error().reason;
}

}  // anonymous namespace

TEST(JsonParserTest, ReadsTheLiterals)
{
    EXPECT_EQ(parse("true"), Value{ true });
    EXPECT_EQ(parse("false"), Value{ false });
    EXPECT_EQ(parse("null"), Value{ Null{} });
}

TEST(JsonParserTest, KeepsANumberAsWritten)
{
    EXPECT_EQ(parse("0"), Value{ Number{ "0" } });
    EXPECT_EQ(parse("-12.50e+3"), Value{ Number{ "-12.50e+3" } });
    EXPECT_EQ(parse("8589934593"), Value{ Number{ "8589934593" } });
    EXPECT_EQ(parse("1E-3"), Value{ Number{ "1E-3" } });
}

TEST(JsonParserTest, RefusesNumbersOutsideTheGrammar)
{
    EXPECT_EQ(reasonFor("01"), "unexpected text after the value");
    EXPECT_EQ(reasonFor("1."), "expected a digit after the decimal point");
    EXPECT_EQ(reasonFor("1e"), "expected a digit in the exponent");
    EXPECT_EQ(reasonFor("-"), "expected a value");
    EXPECT_EQ(reasonFor("+1"), "expected a value");
    EXPECT_EQ(reasonFor(".5"), "expected a value");
}

TEST(JsonParserTest, ResolvesTheEscapesOfAString)
{
    EXPECT_EQ(parse(R"("a\"b\\c\/d\b\f\n\r\t")"), Value{ std::string{ "a\"b\\c/d\b\f\n\r\t" } });
}

TEST(JsonParserTest, EncodesUnicodeEscapesAsUtf8)
{
    EXPECT_EQ(parse(quoted(escape("0041") + escape("00e9") + escape("3042"))), Value{ std::string{ "A\xC3\xA9\xE3\x81\x82" } });
    EXPECT_EQ(parse(quoted(escape("D83D") + escape("DE00"))), Value{ std::string{ "\xF0\x9F\x98\x80" } });
}

TEST(JsonParserTest, RefusesASurrogateWithoutItsPair)
{
    EXPECT_EQ(reasonFor(R"("\uD83D")"), "a high surrogate without a low one");
    EXPECT_EQ(reasonFor(quoted(escape("D83D") + escape("0041"))), "a high surrogate without a low one");
    EXPECT_EQ(reasonFor(R"("\uDE00")"), "a low surrogate without a high one");
}

TEST(JsonParserTest, RefusesBrokenStrings)
{
    EXPECT_EQ(reasonFor(R"("abc)"), "unterminated string");
    EXPECT_EQ(reasonFor(R"("a\q")"), "unknown escape in a string");
    EXPECT_EQ(reasonFor(R"("\u12G4")"), "expected four hex digits after \\u");
    EXPECT_EQ(reasonFor("\"a\nb\""), "control character in a string");
}

TEST(JsonParserTest, PassesTheBytesOfAStringThrough)
{
    EXPECT_EQ(parse("\"\xE3\x81\x82\""), Value{ std::string{ "\xE3\x81\x82" } });
}

TEST(JsonParserTest, ReadsArraysAndObjectsWithWhitespaceAround)
{
    const auto parsed = parse(" { \"a\" : [ 1 , true ] ,\n\t\"b\" : { } , \"c\" : [ ] }\r\n");

    const Value expected{ Object{
        Member{ .name = "a", .value = Value{ Array{ Value{ Number{ "1" } }, Value{ true } } } },
        Member{ .name = "b", .value = Value{ Object{} } },
        Member{ .name = "c", .value = Value{ Array{} } },
    } };
    EXPECT_EQ(parsed, expected);
}

TEST(JsonParserTest, RefusesAnObjectThatNamesAMemberTwice)
{
    const auto parsed = parse(R"({"a": 1, "a": 2})");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().reason, "the member 'a' appears twice");
    EXPECT_EQ(parsed.error().offset, 9U);
}

TEST(JsonParserTest, RefusesBrokenArraysAndObjects)
{
    EXPECT_EQ(reasonFor("[1 2]"), "expected ',' or ']'");
    EXPECT_EQ(reasonFor("[1,]"), "expected a value");
    EXPECT_EQ(reasonFor("{\"a\" 1}"), "expected ':'");
    EXPECT_EQ(reasonFor("{\"a\": 1 \"b\": 2}"), "expected ',' or '}'");
    EXPECT_EQ(reasonFor("{1: 2}"), "expected a member name");
    EXPECT_EQ(reasonFor("{\"a\": 1,}"), "expected a member name");
    EXPECT_EQ(reasonFor("[1"), "expected ',' or ']'");
    EXPECT_EQ(reasonFor("[1}"), "expected ',' or ']'");
    EXPECT_EQ(reasonFor("{\"a\": 1]"), "expected ',' or '}'");
}

TEST(JsonParserTest, RefusesTextOtherThanOneValue)
{
    EXPECT_EQ(reasonFor(""), "expected a value");
    EXPECT_EQ(reasonFor("   "), "expected a value");
    EXPECT_EQ(reasonFor("1 2"), "unexpected text after the value");
    EXPECT_EQ(reasonFor("tru"), "expected a value");
}

TEST(JsonParserTest, SaysWhereTheProblemWasFound)
{
    const auto parsed = parse("[1, x]");

    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().offset, 4U);
}

TEST(JsonParserTest, ReadsNestingUpToTheLimitAndNoDeeper)
{
    const auto nested = [](const std::size_t depth) {
        return std::string(depth, '[') + std::string(depth, ']');
    };

    EXPECT_TRUE(parse(nested(NestingLimit)).has_value());
    EXPECT_EQ(reasonFor(nested(NestingLimit + 1)), "nested too deeply");
    EXPECT_EQ(reasonFor(std::string(100000, '[')), "nested too deeply");

    std::string objects;
    for (std::size_t depth = 0; depth <= NestingLimit; ++depth) {
        objects += "{\"a\":";
    }
    EXPECT_EQ(reasonFor(objects), "nested too deeply");
}
