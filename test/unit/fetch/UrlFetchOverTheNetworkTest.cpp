#include "fetch/CertificateBundle.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "support/TempDirectory.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

using namespace scrap::Fetch;
using scrap::File::readWholeFile;
using scrap::File::writeNewFile;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr std::string_view RealUrl = "https://raw.githubusercontent.com/skipbit/scrap-toolchain/main/index.toml";

/// A self-signed root no public host's certificate leads to.
constexpr std::string_view UnrelatedRoot = "-----BEGIN CERTIFICATE-----\n"
                                           "MIIDCDCCAfCgAwIBAgIUesdGngo2tHGmkekZMTw2YsqmBUowDQYJKoZIhvcNAQEL\n"
                                           "BQAwIzEhMB8GA1UEAwwYVW5yZWxhdGVkIHRlc3QgYXV0aG9yaXR5MCAXDTI2MTEw\n"
                                           "MjEzMDM1MloYDzIxMjYxMDA5MTMwMzUyWjAjMSEwHwYDVQQDDBhVbnJlbGF0ZWQg\n"
                                           "dGVzdCBhdXRob3JpdHkwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCV\n"
                                           "TXzwV+c5Ei8vxnP1F+tErQqjDyigLyUxp8AzlBzXmqO9xsE3Rr/an4aUpicOzUUI\n"
                                           "ch6x2C9pwvfekHSun4KiiRRQutNpylpmDqKWdwkXilmmhYw6fTQDC5Z9Z7fpWE03\n"
                                           "6WkLvpwFs8+fEqUyZvI+O2wDNS+qu9QqqDgEzcgSSzlTRiPAnDy6b9aTZsbBgDyn\n"
                                           "2ejhZxP+FPStFQ7bfsC+3C+Ugn3etnuf5+ksavfp82rFlIfq3OJQ9GAy75Q4pc8V\n"
                                           "lrHMo8lB+5G38uMJcGGa59t6L5EZWva/4JvW6G8VmoFn13OQjaiXzvUJ3k/IHRXP\n"
                                           "JLZrbn4CkRnnswKtsWFNAgMBAAGjMjAwMB0GA1UdDgQWBBQo8+3hoaYMpQDtBzha\n"
                                           "uagiFpXQKjAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBBxIkS\n"
                                           "YDqjJAvQwwsGnYLHWanFeYl/1LMxXYsb/ai4zoUmW59m4oDkHipVIiqj7ti/8bxW\n"
                                           "iIuRGl+5zOLGpsPpo3vfczFY0G9VIBNmVeZ/ohRNaTNH6pGWWr1lBC5TIdK92vEs\n"
                                           "CADbdMnJ06kTvyG7v6i5di71W1l0LJdFEgqeS+ZP2VqRdgg4rP3ye2ED0+rQmd4b\n"
                                           "ynTEmESSOxM5wNKxFTqBnefLonmde8wO8sbqYFY22kO5UOqywvEQUVBvI5R7cwsz\n"
                                           "vvdoRNvZgEW3PQwAMeE8NJV/kmDlAkwfBZfsCSHskObCc74PoHaJ5gByYl0VgEoV\n"
                                           "wi82xEDzN/cI8Om/\n"
                                           "-----END CERTIFICATE-----\n";

/**
 * Tests that reach a real host run only when asked: most runs have no need of
 * the network, and a host being down says nothing about this code.
 */
bool networkTestsAreOn()
{
    const char* value = std::getenv("SCRAP_TEST_NETWORK");
    return (value != nullptr) && (std::string_view{ value } == "1");
}

}  // anonymous namespace

TEST(UrlFetchOverTheNetworkTest, VerifiesARealHostWithTheCertificatesOfThisMachine)
{
    if (! networkTestsAreOn()) {
        GTEST_SKIP() << "set SCRAP_TEST_NETWORK=1 to run";
    }
    const TempDirectory temp;
    const std::optional<std::filesystem::path> bundle = findCertificateBundle("", knownCertificateBundles());
    ASSERT_TRUE(bundle.has_value());

    const auto fetched = fetchToFile({ .url = std::string{ RealUrl }, .destination = temp.path() / "index.toml", .certificateBundle = bundle });

    ASSERT_TRUE(fetched.has_value());
    const auto text = readWholeFile(temp.path() / "index.toml");
    ASSERT_TRUE(text.has_value());
    EXPECT_NE(text->find("schema_version"), std::string::npos);
}

/**
 * The same host fails with a root it does not lead to, which shows the
 * certificates are what let the test above pass.
 */
TEST(UrlFetchOverTheNetworkTest, RefusesARealHostItsCertificatesDoNotVouchFor)
{
    if (! networkTestsAreOn()) {
        GTEST_SKIP() << "set SCRAP_TEST_NETWORK=1 to run";
    }
    const TempDirectory temp;
    const auto unrelated = temp.path() / "unrelated.pem";
    ASSERT_FALSE(writeNewFile(unrelated, UnrelatedRoot).operator bool());

    const auto fetched = fetchToFile({ .url = std::string{ RealUrl }, .destination = temp.path() / "index.toml", .certificateBundle = unrelated });

    ASSERT_FALSE(fetched.has_value());
    EXPECT_TRUE(std::holds_alternative<CertificateFailure>(fetched.error()));
    EXPECT_FALSE(std::filesystem::exists(temp.path() / "index.toml"));
}
