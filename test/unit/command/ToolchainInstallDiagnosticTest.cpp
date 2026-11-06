#include "command/ToolchainInstallDiagnostic.h"

#include "fetch/UrlFetch.h"
#include "install/IndexCopy.h"
#include "install/IngotInstaller.h"
#include "toolchain/IngotSelection.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <system_error>

using namespace scrap::Command;
using namespace scrap::Install;
using scrap::Fetch::CannotWriteDestination;
using scrap::Fetch::CertificateFailure;
using scrap::Fetch::ContentTooLarge;
using scrap::Fetch::NoCertificateBundle;
using scrap::Fetch::TooManyRedirects;
using scrap::Fetch::TransferFailure;
using scrap::Fetch::UrlRefused;
using scrap::Toolchain::NoIngotForHost;
using scrap::Toolchain::NoLongerOffered;
using scrap::Toolchain::NoMatchInIndex;

TEST(ToolchainInstallDiagnosticTest, ShowsAtMostThreeVersions)
{
    const std::string text = renderSelectionFault("gcc@99", NoMatchInIndex{ .versions = { "4.0", "3.0", "2.0", "1.0" }, .families = { "gcc" } });

    EXPECT_EQ(text, "error: no toolchain matches 'gcc@99'\nhint: available versions include 4.0, 3.0, 2.0\n");
}

TEST(ToolchainInstallDiagnosticTest, ShowsTheFamiliesWhenTheFamilyIsUnknown)
{
    const std::string text = renderSelectionFault("nope", NoMatchInIndex{ .versions = {}, .families = { "gcc", "llvm" } });

    EXPECT_EQ(text, "error: no toolchain matches 'nope'\nhint: available toolchains include gcc, llvm\n");
}

TEST(ToolchainInstallDiagnosticTest, SaysSoWhenTheIndexIsEmpty)
{
    const std::string text = renderSelectionFault("gcc", NoMatchInIndex{ .versions = {}, .families = {} });

    EXPECT_EQ(text, "error: no toolchain matches 'gcc'\nhint: the toolchain index lists nothing to install\n");
}

TEST(ToolchainInstallDiagnosticTest, SaysSoWhenNoVersionHasABuildForTheHost)
{
    const std::string text = renderSelectionFault("gcc", NoIngotForHost{ .environment = "darwin-aarch64", .versions = {} });

    EXPECT_EQ(text, "error: 'gcc' has no build for darwin-aarch64\nhint: no version of it is built for this host\n");
}

TEST(ToolchainInstallDiagnosticTest, SaysSoWhenNoVersionCanTakeTheDisabledOnesPlace)
{
    const std::string text = renderSelectionFault("gcc@15", NoLongerOffered{ .family = "gcc", .alternative = std::nullopt });

    EXPECT_EQ(text, "error: 'gcc@15' is no longer provided\nhint: no other version of it can be installed on this host\n");
}

TEST(ToolchainInstallDiagnosticTest, EscapesWhatTheUserTyped)
{
    const std::string text = renderSelectionFault("gcc\x1b", NoIngotForHost{ .environment = "linux-x86_64", .versions = {} });

    EXPECT_EQ(text.find('\x1b'), std::string::npos);
    EXPECT_NE(text.find("gcc\\x1B"), std::string::npos) << text;
}

TEST(ToolchainInstallDiagnosticTest, SendsAMissingCertificateBundleToTheCertificates)
{
    const std::string text = renderIndexFault(IndexUnavailable{ .reason = "no certificates", .fetchError = NoCertificateBundle{ .url = "https://example.com" } });

    EXPECT_EQ(text,
              "error: cannot fetch the toolchain index: no certificates\n"
              "hint: install the root certificates of the system, such as the ca-certificates package, or set SSL_CERT_FILE to a file holding them\n");
}

TEST(ToolchainInstallDiagnosticTest, SendsACertificateThatDoesNotVerifyToTheCertificates)
{
    const std::string text = renderInstallFailure("gcc", IngotNotFetched{ .error = CertificateFailure{ .url = "https://example.com", .message = "bad" } });

    EXPECT_NE(text.find("hint: install the root certificates of the system"), std::string::npos) << text;
}

TEST(ToolchainInstallDiagnosticTest, SendsAFailedTransferToTheNetwork)
{
    const std::string text = renderInstallFailure("gcc", IngotNotFetched{ .error = TransferFailure{ .url = "https://example.com/a", .message = "timed out" } });

    EXPECT_EQ(text, "error: cannot install 'gcc': 'https://example.com/a': timed out\nhint: check the network connection and run the command again\n");
}

TEST(ToolchainInstallDiagnosticTest, DoesNotBlameTheNetworkForContentThatIsTooLarge)
{
    const std::string text = renderInstallFailure("gcc", IngotNotFetched{ .error = ContentTooLarge{ .url = "https://example.com/a", .limit = 5 } });

    EXPECT_EQ(text, "error: cannot install 'gcc': 'https://example.com/a' sent more than 5 bytes\nhint: run the command again; if it keeps failing, report it\n");
}

TEST(ToolchainInstallDiagnosticTest, DoesNotBlameTheNetworkForAUrlItRefuses)
{
    const std::string text = renderInstallFailure("gcc", IngotNotFetched{ .error = UrlRefused{ .url = "http://example.com/a" } });

    EXPECT_NE(text.find("hint: run the command again; if it keeps failing, report it\n"), std::string::npos) << text;
}

TEST(ToolchainInstallDiagnosticTest, SendsTooManyRedirectsToTheNetwork)
{
    const std::string text = renderInstallFailure("gcc", IngotNotFetched{ .error = TooManyRedirects{ .url = "https://example.com/a" } });

    EXPECT_NE(text.find("hint: check the network connection and run the command again\n"), std::string::npos) << text;
}

TEST(ToolchainInstallDiagnosticTest, ChoosesTheHintForAFullDiskByWhatTheSystemSaid)
{
    const std::error_code full = std::make_error_code(std::errc::no_space_on_device);

    EXPECT_NE(renderInstallFailure("gcc", IngotNotPlaced{ .path = "/home/me/.scrap", .code = full }).find("hint: free some disk space and run the command again\n"), std::string::npos);
    EXPECT_NE(renderInstallFailure("gcc", IngotNotUnpacked{ .reason = "cannot write 'a'", .code = full }).find("hint: free some disk space and run the command again\n"), std::string::npos);
    EXPECT_NE(renderInstallFailure("gcc", IngotNotFetched{ .error = CannotWriteDestination{ .path = "/a", .code = full } }).find("hint: free some disk space and run the command again\n"), std::string::npos);
}

TEST(ToolchainInstallDiagnosticTest, SaysWhereTheToolchainThatCouldNotBeBroughtBackStayed)
{
    const std::string text = renderInstallFailure("gcc", IngotNotPlaced{ .path = "/home/me/gcc", .code = std::make_error_code(std::errc::io_error), .previousKeptAt = "/home/me/tmp/replaced" });

    EXPECT_NE(text.find("hint: the toolchain that was installed is at '/home/me/tmp/replaced'; move it back to '/home/me/gcc', since a later install removes it once it is a day old\n"), std::string::npos) << text;
}

TEST(ToolchainInstallDiagnosticTest, DoesNotAskToRetryWhatCannotCrossAFileSystem)
{
    const std::string text = renderInstallFailure("gcc", IngotNotPlaced{ .path = "/home/me/gcc", .code = std::make_error_code(std::errc::cross_device_link) });

    EXPECT_NE(text.find("hint: keep everything below the toolchains directory on one file system\n"), std::string::npos) << text;
}

TEST(ToolchainInstallDiagnosticTest, ChoosesTheHintForAPathRefusedByWhatTheSystemSaid)
{
    const std::error_code refused = std::make_error_code(std::errc::permission_denied);

    EXPECT_NE(renderInstallFailure("gcc", IngotNotPlaced{ .path = "/home/me/.scrap", .code = refused }).find("hint: check the permissions of the path\n"), std::string::npos);
}

TEST(ToolchainInstallDiagnosticTest, AsksForAnUpdateWhenTheIndexFormatIsUnknown)
{
    const std::string text = renderIndexFault(IndexFormatUnsupported{ .format = "2" });

    EXPECT_EQ(text, "error: the toolchain index has the format 2, which this scrap cannot read\nhint: update scrap and run the command again\n");
}

TEST(ToolchainInstallDiagnosticTest, SaysWhatOfTheRegistryCannotBeUsed)
{
    const std::string text = renderInstallFailure("gcc@15", RegistryAnswerUnusable{ .url = "https://ghcr.io/v2/x/gcc/manifests/sha256:abc", .reason = "it gave no token" });

    EXPECT_EQ(text,
              "error: cannot install 'gcc@15': the registry answered 'https://ghcr.io/v2/x/gcc/manifests/sha256:abc' with what scrap cannot use: it gave no token\n"
              "hint: run the command again; if it keeps failing, report it\n");
}

TEST(ToolchainInstallDiagnosticTest, SaysWhichRegistryAndTagSayNotWhereTheBuildIsKept)
{
    const std::string text = renderInstallFailure("gcc@15", RegistryNotLocated{ .registry = "ghcr.io/x", .tag = "gcc" });

    EXPECT_EQ(text,
              "error: cannot install 'gcc@15': the index names the registry 'ghcr.io/x' and the tag 'gcc', which do not say where its build is kept\n"
              "hint: run the command again; if it keeps failing, report it\n");
}
