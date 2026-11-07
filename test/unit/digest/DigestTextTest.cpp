#include "digest/DigestText.h"

#include <gtest/gtest.h>

#include <string>

using scrap::Digest::isSha256Digest;
using scrap::Digest::isSha256Hex;
using scrap::Digest::sha256Digest;

namespace {

const std::string Hex(64, 'a');

}  // anonymous namespace

TEST(DigestTextTest, AcceptsSixtyFourLowercaseHexDigits)
{
    EXPECT_TRUE(isSha256Hex(Hex));
    EXPECT_TRUE(isSha256Hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
}

TEST(DigestTextTest, RefusesHexThatIsUppercaseOrOfAnotherLength)
{
    EXPECT_FALSE(isSha256Hex(std::string(64, 'A')));
    EXPECT_FALSE(isSha256Hex(std::string(63, 'a')));
    EXPECT_FALSE(isSha256Hex(std::string(65, 'a')));
    EXPECT_FALSE(isSha256Hex(std::string(63, 'a') + "g"));
    EXPECT_FALSE(isSha256Hex(""));
}

TEST(DigestTextTest, AcceptsADigestWithItsPrefix)
{
    EXPECT_TRUE(isSha256Digest("sha256:" + Hex));
}

TEST(DigestTextTest, RefusesADigestWithoutItsPrefixOrWithAnotherOne)
{
    EXPECT_FALSE(isSha256Digest(Hex));
    EXPECT_FALSE(isSha256Digest("sha512:" + Hex));
    EXPECT_FALSE(isSha256Digest("SHA256:" + Hex));
}

TEST(DigestTextTest, RefusesADigestWhoseDigitsAreUppercaseOrOfAnotherLength)
{
    EXPECT_FALSE(isSha256Digest("sha256:" + std::string(64, 'A')));
    EXPECT_FALSE(isSha256Digest("sha256:" + std::string(63, 'a')));
    EXPECT_FALSE(isSha256Digest("sha256:" + std::string(65, 'a')));
}

TEST(DigestTextTest, ComposesADigestFromItsDigits)
{
    EXPECT_EQ(sha256Digest(Hex), "sha256:" + Hex);
    EXPECT_TRUE(isSha256Digest(sha256Digest(Hex)));
}
