#include "json/JsonValue.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

using scrap::Json::Array;
using scrap::Json::Member;
using scrap::Json::Null;
using scrap::Json::Number;
using scrap::Json::Object;
using scrap::Json::Value;

TEST(JsonValueTest, FindsAMemberByName)
{
    const Value object{ Object{ Member{ .name = "a", .value = Value{ true } }, Member{ .name = "b", .value = Value{ Null{} } } } };

    ASSERT_NE(object.member("b"), nullptr);
    EXPECT_EQ(*object.member("b"), Value{ Null{} });
    EXPECT_EQ(object.member("c"), nullptr);
}

TEST(JsonValueTest, FindsNoMemberInWhatIsNotAnObject)
{
    EXPECT_EQ(Value{ Array{} }.member("a"), nullptr);
    EXPECT_EQ(Value{ std::string{ "a" } }.member("a"), nullptr);
}

TEST(JsonValueTest, GivesAStringOrAnArrayOnlyWhenItIsOne)
{
    const Value text{ std::string{ "x" } };
    const Value items{ Array{ Value{ true } } };

    ASSERT_NE(text.asString(), nullptr);
    EXPECT_EQ(*text.asString(), "x");
    EXPECT_EQ(text.asArray(), nullptr);
    ASSERT_NE(items.asArray(), nullptr);
    EXPECT_EQ(items.asArray()->size(), 1U);
    EXPECT_EQ(items.asString(), nullptr);
}

TEST(JsonValueTest, GivesAMemberOnlyWhenItIsAString)
{
    const Value object{ Object{ Member{ .name = "a", .value = Value{ std::string{ "x" } } }, Member{ .name = "b", .value = Value{ true } } } };

    ASSERT_NE(object.stringMember("a"), nullptr);
    EXPECT_EQ(*object.stringMember("a"), "x");
    EXPECT_EQ(object.stringMember("b"), nullptr);
    EXPECT_EQ(object.stringMember("c"), nullptr);
    EXPECT_EQ(Value{ std::string{ "x" } }.stringMember("a"), nullptr);
}

TEST(JsonValueTest, GivesAWholeNumberThatFits)
{
    EXPECT_EQ(Value{ Number{ "0" } }.asUnsigned(), std::optional<std::uint64_t>{ 0 });
    EXPECT_EQ(Value{ Number{ "8589934593" } }.asUnsigned(), std::optional<std::uint64_t>{ 8589934593U });
    EXPECT_EQ(Value{ Number{ "18446744073709551615" } }.asUnsigned(), std::optional<std::uint64_t>{ UINT64_MAX });
}

TEST(JsonValueTest, GivesNoWholeNumberForOtherNumbersOrValues)
{
    EXPECT_EQ(Value{ Number{ "18446744073709551616" } }.asUnsigned(), std::nullopt);
    EXPECT_EQ(Value{ Number{ "-1" } }.asUnsigned(), std::nullopt);
    EXPECT_EQ(Value{ Number{ "1.0" } }.asUnsigned(), std::nullopt);
    EXPECT_EQ(Value{ Number{ "1e3" } }.asUnsigned(), std::nullopt);
    EXPECT_EQ(Value{ std::string{ "1" } }.asUnsigned(), std::nullopt);
}
