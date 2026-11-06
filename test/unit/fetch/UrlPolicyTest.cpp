#include "fetch/UrlPolicy.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace scrap::Fetch;

namespace {

struct HostCase {
    std::string_view host;
    bool thisMachine;
};

struct TargetCase {
    std::string scheme;
    std::string host;
    bool accepted;
    bool verified;
};

struct RedirectCase {
    UrlTarget from;
    UrlTarget to;
    bool accepted;
};

}  // anonymous namespace

TEST(UrlPolicyTest, RecognisesTheNamesOfThisMachine)
{
    const std::vector<HostCase> cases{
        { "127.0.0.1", true },
        { "[::1]", true },
        { "localhost", true },
        { "::1", false },
        { "127.0.0.2", false },
        { "localhost.example.com", false },
        { "example.com", false },
        { "LOCALHOST", false },
        { "", false },
    };

    for (const auto& [host, thisMachine] : cases) {
        EXPECT_EQ(isThisMachine(host), thisMachine) << "host: " << host;
    }
}

TEST(UrlPolicyTest, AcceptsHttpsAnywhereAndHttpOnlyFromThisMachine)
{
    const std::vector<TargetCase> cases{
        { "https", "example.com", true, true },
        { "https", "localhost", true, true },
        { "http", "localhost", true, false },
        { "http", "127.0.0.1", true, false },
        { "http", "[::1]", true, false },
        { "http", "example.com", false, false },
        { "ftp", "localhost", false, false },
        { "file", "localhost", false, false },
        { "ftp", "example.com", false, false },
    };

    for (const auto& [scheme, host, accepted, verified] : cases) {
        const UrlTarget target{ .scheme = scheme, .host = host };
        EXPECT_EQ(isAccepted(target), accepted) << scheme << "://" << host;
        EXPECT_EQ(needsCertificates(target), verified) << scheme << "://" << host;
    }
}

TEST(UrlPolicyTest, LetsARedirectReachHttpOnlyFromThisMachine)
{
    const std::vector<RedirectCase> cases{
        { { "http", "localhost" }, { "http", "127.0.0.1" }, true },
        { { "https", "localhost" }, { "http", "localhost" }, true },
        { { "http", "localhost" }, { "https", "example.com" }, true },
        { { "https", "example.com" }, { "https", "example.org" }, true },
        { { "https", "example.com" }, { "http", "127.0.0.1" }, false },
        { { "https", "example.com" }, { "http", "localhost" }, false },
        { { "https", "example.com" }, { "http", "[::1]" }, false },
        { { "https", "example.com" }, { "http", "example.org" }, false },
        { { "http", "localhost" }, { "http", "example.com" }, false },
        { { "http", "localhost" }, { "ftp", "localhost" }, false },
    };

    for (const auto& [from, to, accepted] : cases) {
        EXPECT_EQ(isAcceptedRedirect(from, to), accepted) << from.scheme << "://" << from.host << " to " << to.scheme << "://" << to.host;
    }
}

TEST(UrlPolicyTest, TakesTheSameSchemeHostAndPortAsTheSameServer)
{
    const UrlTarget origin{ .scheme = "https", .host = "ghcr.io", .port = "443" };

    EXPECT_TRUE(isSameOrigin(origin, UrlTarget{ .scheme = "https", .host = "ghcr.io", .port = "443" }));
    EXPECT_FALSE(isSameOrigin(origin, UrlTarget{ .scheme = "http", .host = "ghcr.io", .port = "443" }));
    EXPECT_FALSE(isSameOrigin(origin, UrlTarget{ .scheme = "https", .host = "pkg-containers.githubusercontent.com", .port = "443" }));
    EXPECT_FALSE(isSameOrigin(origin, UrlTarget{ .scheme = "https", .host = "ghcr.io", .port = "8443" }));
}
