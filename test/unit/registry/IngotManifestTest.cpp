#include "registry/IngotManifest.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using scrap::Registry::readIngotManifest;

namespace {

constexpr std::string_view LayerDigest = "sha256:9e75613dfa9d8f240ce5ba5bad84b3898b7208fe16374f61e2a1747d52b0a683";

/// A manifest of the form a registry serves, with @p layers in place of its layers.
std::string manifestWith(const std::string& layers, const std::string& mediaType = "application/vnd.oci.image.manifest.v1+json")
{
    return R"({"schemaVersion": 2, "mediaType": ")" + mediaType + R"(", "artifactType": "application/vnd.skipbit.scrap.ingot.v1+tar.xz",
        "config": {"mediaType": "application/vnd.oci.empty.v1+json", "digest": "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a", "size": 2},
        "layers": )"
           + layers + "}";
}

std::string layer(const std::string& mediaType, const std::string& digest, const std::string& size)
{
    return R"({"mediaType": ")" + mediaType + R"(", "digest": ")" + digest + R"(", "size": )" + size + "}";
}

std::string xzLayer()
{
    return layer("application/x-tar+xz", std::string{ LayerDigest }, "8589934593");
}

std::string reasonFor(const std::string& manifest)
{
    const auto read = readIngotManifest(manifest);
    return read.has_value() ? std::string{ "(read)" } : read.error();
}

}  // anonymous namespace

TEST(IngotManifestTest, ReadsTheOneLayerOfAnIngot)
{
    const auto read = readIngotManifest(manifestWith("[" + xzLayer() + "]"));

    ASSERT_TRUE(read.has_value()) << read.error();
    EXPECT_EQ(read->digest, LayerDigest);
    EXPECT_EQ(read->size, 8589934593U);
}

TEST(IngotManifestTest, RefusesWhatIsNotAnImageManifest)
{
    EXPECT_EQ(reasonFor("{"), "it is not JSON: expected a member name at byte 1");
    EXPECT_EQ(reasonFor(manifestWith("[" + xzLayer() + "]", "application/vnd.oci.image.index.v1+json")), "it is not application/vnd.oci.image.manifest.v1+json");
    EXPECT_EQ(reasonFor(R"({"layers": [)" + xzLayer() + "]}"), "it is not application/vnd.oci.image.manifest.v1+json");
}

TEST(IngotManifestTest, RefusesAnythingButExactlyOneLayer)
{
    EXPECT_EQ(reasonFor(manifestWith("[]")), "it does not hold exactly one layer");
    EXPECT_EQ(reasonFor(manifestWith("[" + xzLayer() + "," + xzLayer() + "]")), "it does not hold exactly one layer");
    EXPECT_EQ(reasonFor(manifestWith(xzLayer())), "it does not hold exactly one layer");
    EXPECT_EQ(reasonFor(R"({"mediaType": "application/vnd.oci.image.manifest.v1+json"})"), "it does not hold exactly one layer");
}

TEST(IngotManifestTest, RefusesALayerThatIsNotAnXzTarArchive)
{
    EXPECT_EQ(reasonFor(manifestWith("[" + layer("application/vnd.oci.image.layer.v1.tar+gzip", std::string{ LayerDigest }, "1") + "]")), "its layer is not application/x-tar+xz");
}

TEST(IngotManifestTest, RefusesALayerWithoutASha256DigestOrASize)
{
    const std::string upper = "sha256:9E75613DFA9D8F240CE5BA5BAD84B3898B7208FE16374F61E2A1747D52B0A683";
    const std::string shortDigest = std::string{ LayerDigest }.substr(0, LayerDigest.size() - 1);
    const std::string otherAlgorithm = "sha512:" + std::string{ LayerDigest }.substr(7);

    EXPECT_EQ(reasonFor(manifestWith("[" + layer("application/x-tar+xz", upper, "1") + "]")), "its layer has no SHA-256 digest");
    EXPECT_EQ(reasonFor(manifestWith("[" + layer("application/x-tar+xz", shortDigest, "1") + "]")), "its layer has no SHA-256 digest");
    EXPECT_EQ(reasonFor(manifestWith("[" + layer("application/x-tar+xz", otherAlgorithm, "1") + "]")), "its layer has no SHA-256 digest");
    EXPECT_EQ(reasonFor(manifestWith("[" + layer("application/x-tar+xz", std::string{ LayerDigest }, "-1") + "]")), "its layer has no size");
    EXPECT_EQ(reasonFor(manifestWith("[" + layer("application/x-tar+xz", std::string{ LayerDigest }, "\"1\"") + "]")), "its layer has no size");
    EXPECT_EQ(reasonFor(manifestWith(R"([{"mediaType": "application/x-tar+xz", "size": 1}])")), "its layer has no SHA-256 digest");
}
