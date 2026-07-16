#include <gtest/gtest.h>

#include "command/MetadataProtocolProvider.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace scrap::Command;

namespace {

/**
 * Timeout used by tests that expect a fast, successful (or cleanly failed)
 * probe. Short enough to keep the suite fast, long enough to never be
 * mistaken for the deliberately short timeout used by the Timeout test.
 */
constexpr std::chrono::milliseconds FastTimeout{2000};

}  // namespace

/**
 * Test fixture providing a temp directory for provider tests.
 */
class MetadataProtocolProviderTest : public ::testing::Test {
protected:
    /**
     * Create a per-test temp directory.
     *
     * Each test case runs as its own ctest entry and ctest may run them
     * in parallel, so the directory name must be unique per test case.
     */
    void SetUp() override
    {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir_ = std::filesystem::temp_directory_path() / (std::string("scrap_provider_test_") + info->name());
        std::filesystem::remove_all(tempDir_);
        std::filesystem::create_directories(tempDir_);
    }

    /**
     * Clean up the temp directory.
     */
    void TearDown() override
    {
        std::filesystem::remove_all(tempDir_);
    }

    /**
     * Create a shell script with execute permission.
     */
    auto createExecutable(const std::string& name, const std::string& body) -> std::filesystem::path
    {
        auto path = tempDir_ / name;
        std::ofstream(path) << "#!/bin/sh\n" << body << "\n";
        std::filesystem::permissions(path, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
        return path;
    }

    std::filesystem::path tempDir_;
};

/**
 * Verify that a description is fetched from --scrap-metadata output.
 */
TEST_F(MetadataProtocolProviderTest, MetadataViaProtocol)
{
    auto script = createExecutable("scrap-x", R"(if [ "$1" = "--scrap-metadata" ]; then
  echo "Hello desc"
  exit 0
fi
exit 1)");

    MetadataProtocolProvider provider(FastTimeout);
    auto result = provider.fetch(script);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->description, "Hello desc");
    EXPECT_TRUE(result->name.empty());
}

/**
 * Verify that --help output is used when --scrap-metadata fails.
 */
TEST_F(MetadataProtocolProviderTest, FallbackToHelp)
{
    auto script = createExecutable("scrap-x", R"(if [ "$1" = "--scrap-metadata" ]; then
  exit 1
fi
if [ "$1" = "--help" ]; then
  echo "Help line"
  exit 0
fi
exit 1)");

    MetadataProtocolProvider provider(FastTimeout);
    auto result = provider.fetch(script);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->description, "Help line");
}

/**
 * Verify that failure of both --scrap-metadata and --help yields an error.
 */
TEST_F(MetadataProtocolProviderTest, BothFailUnexpected)
{
    auto script = createExecutable("scrap-x", "exit 1");

    MetadataProtocolProvider provider(FastTimeout);
    auto result = provider.fetch(script);

    EXPECT_FALSE(result.has_value());
}

/**
 * Verify that a hung child is killed and fetch() fails within the injected
 * timeout, rather than blocking for the child's full runtime.
 */
TEST_F(MetadataProtocolProviderTest, Timeout)
{
    auto script = createExecutable("scrap-x", "sleep 10");

    constexpr std::chrono::milliseconds shortTimeout{200};
    MetadataProtocolProvider provider(shortTimeout);

    auto start = std::chrono::steady_clock::now();
    auto result = provider.fetch(script);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    // Two probe attempts (--scrap-metadata, --help) each bounded by
    // shortTimeout; generous upper bound keeps this robust under CI load
    // while still proving we did not wait anywhere near the 10s sleep.
    EXPECT_LT(elapsed, std::chrono::seconds(5));
}

/**
 * Verify that a child which emits more than the capture cap and then hangs
 * is still killed and reaped promptly. Regression for a bug where the kill
 * was gated on "the drain timed out", so hitting the capture cap (a
 * distinct stop reason) skipped the kill and waitpid() blocked for the
 * child's full lifetime.
 */
TEST_F(MetadataProtocolProviderTest, CapThenHang)
{
    // MaxCaptureBytes is 64 * 1024; 70000 bytes safely exceeds it for both
    // probe attempts.
    auto script = createExecutable("scrap-x", R"(case "$1" in
  --scrap-metadata|--help) yes x | head -c 70000; sleep 30 ;;
esac
exit 1)");

    constexpr std::chrono::milliseconds shortTimeout{200};
    MetadataProtocolProvider provider(shortTimeout);

    auto start = std::chrono::steady_clock::now();
    auto result = provider.fetch(script);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    // Two probe attempts each bounded by shortTimeout; generous upper bound
    // keeps this robust under CI load while still proving the capture-cap
    // path no longer blocks for the child's 30s sleep.
    EXPECT_LT(elapsed, std::chrono::seconds(5));
}

/**
 * Verify that a child which closes stdout (EOF) but keeps running is still
 * killed and reaped promptly. Regression for a bug where the kill was
 * gated on "the drain timed out"; plain EOF is a distinct stop reason that
 * also skipped the kill, so a child that closes stdout and then keeps
 * running could hang waitpid() indefinitely — the security-flagged variant
 * (a bad `scrap-*` plugin can be probed on every `scrap --help`).
 */
TEST_F(MetadataProtocolProviderTest, ClosesStdoutThenHang)
{
    auto script = createExecutable("scrap-x", "exec 1>&-; sleep 30");

    constexpr std::chrono::milliseconds shortTimeout{200};
    MetadataProtocolProvider provider(shortTimeout);

    auto start = std::chrono::steady_clock::now();
    auto result = provider.fetch(script);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.has_value());
    // EOF is observed immediately (stdout is closed), but the child keeps
    // running; fetch() must still return within a small multiple of
    // shortTimeout, proving the EOF path no longer blocks for the child's
    // 30s sleep.
    EXPECT_LT(elapsed, std::chrono::seconds(5));
}

/**
 * Verify that a valid child which closes stdout, does a bit of brief work,
 * and then exits 0 is NOT force-failed. Regression for a bug where the
 * reap unconditionally SIGKILLed the process group right after the drain
 * saw EOF: EOF only proves stdout's write end is closed, not that the
 * child has exited, so a still-alive-but-about-to-exit child raced the
 * kill and its already-captured (valid) output was discarded in favor of
 * a bogus killed/WIFSIGNALED status.
 */
TEST_F(MetadataProtocolProviderTest, ValidProviderClosingStdoutEarly)
{
    auto script = createExecutable("scrap-x", R"(if [ "$1" = "--scrap-metadata" ]; then
  echo "early-close-desc"
  exec 1>&-
  sleep 0.1
  exit 0
fi
exit 1)");

    MetadataProtocolProvider provider(FastTimeout);
    auto result = provider.fetch(script);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->description, "early-close-desc");
}

/**
 * Verify that a missing or non-executable path fails cleanly (no crash).
 */
TEST_F(MetadataProtocolProviderTest, NonExecutableOrMissing)
{
    MetadataProtocolProvider provider(FastTimeout);

    auto missingResult = provider.fetch(tempDir_ / "does-not-exist");
    EXPECT_FALSE(missingResult.has_value());

    auto nonExecPath = tempDir_ / "scrap-not-exec";
    std::ofstream(nonExecPath) << "#!/bin/sh\necho unreachable\n";
    auto nonExecResult = provider.fetch(nonExecPath);
    EXPECT_FALSE(nonExecResult.has_value());
}

/**
 * Verify that exit-0-but-empty output from both attempts yields an error.
 */
TEST_F(MetadataProtocolProviderTest, EmptyOutput)
{
    auto script = createExecutable("scrap-x", R"(if [ "$1" = "--scrap-metadata" ]; then
  exit 0
fi
exit 1)");

    MetadataProtocolProvider provider(FastTimeout);
    auto result = provider.fetch(script);

    EXPECT_FALSE(result.has_value());
}
