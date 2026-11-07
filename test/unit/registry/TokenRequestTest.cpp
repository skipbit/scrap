#include "registry/TokenRequest.h"

#include "registry/BearerChallenge.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using scrap::Registry::BearerChallenge;
using scrap::Registry::readTokenAnswer;
using scrap::Registry::tokenUrl;

TEST(TokenRequestTest, AsksTheRealmForTheServiceAndTheScope)
{
    const BearerChallenge challenge{ .realm = "https://ghcr.io/token", .service = "ghcr.io", .scope = "repository:a/b:pull" };

    EXPECT_EQ(tokenUrl(challenge, "repository:x/y:pull"), "https://ghcr.io/token?service=ghcr.io&scope=repository%3Aa%2Fb%3Apull");
}

TEST(TokenRequestTest, LeavesOutTheServiceTheChallengeDoesNotName)
{
    const BearerChallenge challenge{ .realm = "https://example.com/token", .service = "", .scope = "s" };

    EXPECT_EQ(tokenUrl(challenge, "d"), "https://example.com/token?scope=s");
}

TEST(TokenRequestTest, AddsToTheQueryTheRealmAlreadyHas)
{
    const BearerChallenge challenge{ .realm = "https://example.com/token?account=me", .service = "svc", .scope = "s" };

    EXPECT_EQ(tokenUrl(challenge, "d"), "https://example.com/token?account=me&service=svc&scope=s");
}

TEST(TokenRequestTest, AsksForTheDefaultScopeWhenTheChallengeNamesNone)
{
    const BearerChallenge challenge{ .realm = "https://example.com/token", .service = "", .scope = "" };

    EXPECT_EQ(tokenUrl(challenge, "repository:x/y:pull"), "https://example.com/token?scope=repository%3Ax%2Fy%3Apull");
}

TEST(TokenRequestTest, PercentEncodesWhatIsNotUnreserved)
{
    const BearerChallenge challenge{ .realm = "https://example.com/token", .service = "a b&c=d", .scope = "A-z_0.9~:/ " };

    EXPECT_EQ(tokenUrl(challenge, "d"), "https://example.com/token?service=a%20b%26c%3Dd&scope=A-z_0.9~%3A%2F%20");
}

TEST(TokenRequestTest, ReadsTheTokenOfAnAnswer)
{
    EXPECT_EQ(readTokenAnswer(R"({"token":"abc","expires_in":300})"), std::optional<std::string>{ "abc" });
}

TEST(TokenRequestTest, ReadsTheAccessTokenWhenThereIsNoToken)
{
    EXPECT_EQ(readTokenAnswer(R"({"access_token":"xyz"})"), std::optional<std::string>{ "xyz" });
}

TEST(TokenRequestTest, PrefersTheTokenToTheAccessToken)
{
    EXPECT_EQ(readTokenAnswer(R"({"access_token":"xyz","token":"abc"})"), std::optional<std::string>{ "abc" });
}

TEST(TokenRequestTest, FallsBackToTheAccessTokenWhenTheTokenIsEmpty)
{
    EXPECT_EQ(readTokenAnswer(R"({"token":"","access_token":"xyz"})"), std::optional<std::string>{ "xyz" });
}

TEST(TokenRequestTest, GivesNoTokenWhenBothAreEmpty)
{
    EXPECT_EQ(readTokenAnswer(R"({"token":"","access_token":""})"), std::nullopt);
}

TEST(TokenRequestTest, GivesNoTokenThatIsNotAString)
{
    EXPECT_EQ(readTokenAnswer(R"({"token":42})"), std::nullopt);
    EXPECT_EQ(readTokenAnswer(R"({"token":null,"access_token":["x"]})"), std::nullopt);
}

TEST(TokenRequestTest, GivesNoTokenFromWhatIsNotJson)
{
    EXPECT_EQ(readTokenAnswer("token=abc"), std::nullopt);
    EXPECT_EQ(readTokenAnswer(""), std::nullopt);
}
