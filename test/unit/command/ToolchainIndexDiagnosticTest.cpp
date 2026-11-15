#include "command/ToolchainIndexDiagnostic.h"

#include "fetch/UrlFetch.h"
#include "install/IndexCopy.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace scrap::Command;
using namespace scrap::Install;
using scrap::Fetch::NoCertificateBundle;

TEST(ToolchainIndexDiagnosticTest, SendsAMissingCertificateBundleToTheCertificates)
{
    const std::string text = renderIndexFault(IndexUnavailable{ .reason = "no certificates", .fetchError = NoCertificateBundle{ .url = "https://example.com" } });

    EXPECT_EQ(text,
              "error: cannot fetch the toolchain index: no certificates\n"
              "hint: install the root certificates of the system, such as the ca-certificates package, or set SSL_CERT_FILE to a file holding them\n");
}

TEST(ToolchainIndexDiagnosticTest, AsksForAnUpdateWhenTheIndexFormatIsUnknown)
{
    const std::string text = renderIndexFault(IndexFormatUnsupported{ .format = "2" });

    EXPECT_EQ(text, "error: the toolchain index has the format 2, which this scrap cannot read\nhint: update scrap and run the command again\n");
}

TEST(ToolchainIndexDiagnosticTest, SaysTheCopyIsKeptWhenTheIndexCannotBeFetched)
{
    EXPECT_EQ(renderCopyKept("the server answered with status 500", "2026-11-14T09:00:00Z"),
              "error: cannot fetch the toolchain index: the server answered with status 500\n"
              "hint: the copy fetched at 2026-11-14T09:00:00Z is kept as it was\n");
}

TEST(ToolchainIndexDiagnosticTest, SaysWhenTheFetchTimeIsKnownOrNot)
{
    EXPECT_EQ(fetchTimeText("2026-11-15T10:00:00Z"), "2026-11-15T10:00:00Z");
    EXPECT_EQ(fetchTimeText(std::nullopt), "an unknown time");
}
