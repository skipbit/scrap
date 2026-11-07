#include "registry/BearerChallenge.h"

#include <gtest/gtest.h>

#include <optional>

using scrap::Registry::readBearerChallenge;

TEST(BearerChallengeTest, ReadsTheChallengeOfARegistry)
{
    const auto challenge = readBearerChallenge(R"(Bearer realm="https://ghcr.io/token",service="ghcr.io",scope="repository:skipbit/scrap-toolchain/gcc:pull")");

    ASSERT_TRUE(challenge.has_value());
    EXPECT_EQ(challenge->realm, "https://ghcr.io/token");
    EXPECT_EQ(challenge->service, "ghcr.io");
    EXPECT_EQ(challenge->scope, "repository:skipbit/scrap-toolchain/gcc:pull");
}

TEST(BearerChallengeTest, TakesTheSchemeAndNamesInAnyCaseAndValuesUnquoted)
{
    const auto challenge = readBearerChallenge(R"( bearer  REALM = "https://example.com/token" , Service=registry )");

    ASSERT_TRUE(challenge.has_value());
    EXPECT_EQ(challenge->realm, "https://example.com/token");
    EXPECT_EQ(challenge->service, "registry");
    EXPECT_EQ(challenge->scope, "");
}

TEST(BearerChallengeTest, ResolvesEscapesAndPassesOverOtherParameters)
{
    const auto challenge = readBearerChallenge(R"(Bearer error="insufficient_scope",realm="https://example.com/a\"b",scope="x,y")");

    ASSERT_TRUE(challenge.has_value());
    EXPECT_EQ(challenge->realm, "https://example.com/a\"b");
    EXPECT_EQ(challenge->scope, "x,y");
}

TEST(BearerChallengeTest, RefusesOtherSchemes)
{
    EXPECT_EQ(readBearerChallenge(R"(Basic realm="registry")"), std::nullopt);
    EXPECT_EQ(readBearerChallenge(R"(Bearerx realm="registry")"), std::nullopt);
    EXPECT_EQ(readBearerChallenge(""), std::nullopt);
}

TEST(BearerChallengeTest, RefusesAChallengeWithoutARealm)
{
    EXPECT_EQ(readBearerChallenge(R"(Bearer service="ghcr.io")"), std::nullopt);
    EXPECT_EQ(readBearerChallenge("Bearer"), std::nullopt);
    EXPECT_EQ(readBearerChallenge(R"(Bearer realm="")"), std::nullopt);
}

TEST(BearerChallengeTest, RefusesAChallengeItCannotRead)
{
    EXPECT_EQ(readBearerChallenge(R"(Bearer realm="https://example.com/token)"), std::nullopt);
    EXPECT_EQ(readBearerChallenge(R"(Bearer realm="https://example.com/token" service="x")"), std::nullopt);
    EXPECT_EQ(readBearerChallenge(R"(Bearer realm)"), std::nullopt);
    EXPECT_EQ(readBearerChallenge(R"(Bearer realm="a\)"), std::nullopt);
}
