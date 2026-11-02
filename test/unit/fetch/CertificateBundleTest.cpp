#include "fetch/CertificateBundle.h"

#include "file/WholeFile.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

using namespace scrap::Fetch;
using scrap::TestSupport::TempDirectory;

TEST(CertificateBundleTest, UsesTheFirstKnownPlaceHoldingAFile)
{
    const TempDirectory temp;
    const std::string missing = (temp.path() / "missing").string();
    const std::string first = (temp.path() / "first").string();
    const std::string second = (temp.path() / "second").string();
    ASSERT_FALSE(scrap::File::writeNewFile(first, "").operator bool());
    ASSERT_FALSE(scrap::File::writeNewFile(second, "").operator bool());
    const std::array<std::string_view, 3> known{ missing, first, second };

    EXPECT_EQ(findCertificateBundle("", known), first);
}

TEST(CertificateBundleTest, PassesOverADirectoryInAKnownPlace)
{
    const TempDirectory temp;
    const std::string file = (temp.path() / "file").string();
    ASSERT_FALSE(scrap::File::writeNewFile(file, "").operator bool());
    const std::string directory = temp.path().string();
    const std::array<std::string_view, 2> known{ directory, file };

    EXPECT_EQ(findCertificateBundle("", known), file);
}

TEST(CertificateBundleTest, FindsNothingWhenNoKnownPlaceHoldsAFile)
{
    const TempDirectory temp;
    const std::string missing = (temp.path() / "missing").string();
    const std::array<std::string_view, 1> known{ missing };

    EXPECT_EQ(findCertificateBundle("", known), std::nullopt);
}

TEST(CertificateBundleTest, UsesTheFileTheUserNamedEvenWhenItIsNotThere)
{
    const TempDirectory temp;
    const std::string file = (temp.path() / "file").string();
    ASSERT_FALSE(scrap::File::writeNewFile(file, "").operator bool());
    const std::array<std::string_view, 1> known{ file };

    EXPECT_EQ(findCertificateBundle("/named/by/the/user.pem", known), "/named/by/the/user.pem");
}

TEST(CertificateBundleTest, KnowsThePlacesOfTheDistributionsMeasured)
{
    const auto known = knownCertificateBundles();
    for (const std::string_view place : { "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/cert.pem" }) {
        EXPECT_NE(std::ranges::find(known, place), known.end()) << place;
    }
}
