#include "digest/Sha256.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

using scrap::Digest::Sha256;

namespace {

std::string digestOf(std::string_view bytes)
{
    Sha256 sha256;
    sha256.update(bytes);
    return sha256.hexDigest();
}

}  // anonymous namespace

// The expected digests come from sha256sum.

TEST(Sha256Test, DigestsAnEmptyMessage)
{
    EXPECT_EQ(Sha256{}.hexDigest(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, DigestsAMessageOfOneBlock)
{
    EXPECT_EQ(digestOf("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, DigestsAMessageWhoseLengthNeedsASecondBlock)
{
    EXPECT_EQ(digestOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256Test, DigestsAMessageOfTwoBlocks)
{
    EXPECT_EQ(digestOf("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"), "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

TEST(Sha256Test, DigestsAMillionBytes)
{
    EXPECT_EQ(digestOf(std::string(1'000'000, 'a')), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256Test, PadsMessagesAroundTheBlockBoundary)
{
    EXPECT_EQ(digestOf(std::string(55, 'a')), "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    EXPECT_EQ(digestOf(std::string(56, 'a')), "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    EXPECT_EQ(digestOf(std::string(63, 'a')), "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    EXPECT_EQ(digestOf(std::string(64, 'a')), "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    EXPECT_EQ(digestOf(std::string(65, 'a')), "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0");
    EXPECT_EQ(digestOf(std::string(119, 'a')), "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb");
    EXPECT_EQ(digestOf(std::string(120, 'a')), "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c");
}

TEST(Sha256Test, GivesTheSameDigestHoweverTheBytesAreSplit)
{
    const std::string message(1000, 'a');
    const std::string expected = digestOf(message);

    for (const std::size_t pieceSize : { std::size_t{ 1 }, std::size_t{ 63 }, std::size_t{ 64 }, std::size_t{ 65 }, std::size_t{ 999 } }) {
        Sha256 sha256;
        for (std::size_t start = 0; start < message.size(); start += pieceSize) {
            sha256.update(std::string_view{ message }.substr(start, pieceSize));
        }
        EXPECT_EQ(sha256.hexDigest(), expected) << "pieces of " << pieceSize;
    }
}

TEST(Sha256Test, TakesMoreBytesAfterADigest)
{
    Sha256 sha256;
    sha256.update("ab");
    EXPECT_EQ(sha256.hexDigest(), digestOf("ab"));

    sha256.update("c");
    EXPECT_EQ(sha256.hexDigest(), digestOf("abc"));
}
