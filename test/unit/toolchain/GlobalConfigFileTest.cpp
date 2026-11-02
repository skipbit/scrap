#include "toolchain/GlobalConfigFile.h"

#include "support/TempDirectory.h"
#include "toolchain/ScrapHomeLayout.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

using namespace scrap::Toolchain;
using scrap::TestSupport::TempDirectory;

/**
 * A file that is not there holds no default.
 */
TEST(GlobalConfigFileTest, ReadsAMissingFileAsHoldingNothing)
{
    const TempDirectory home;

    const auto read = readGlobalDefaultFile(globalConfigFile(home.path()));

    ASSERT_TRUE(read.has_value());
    EXPECT_FALSE(read->has_value());
}

/**
 * The default is read as written.
 */
TEST(GlobalConfigFileTest, ReadsTheDefaultTheFileHolds)
{
    const TempDirectory home;
    const auto file = home.writeFile("config.toml", "[defaults]\ntoolchain = \"gcc@15.2.0\"\n");

    const auto read = readGlobalDefaultFile(file);

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, std::optional<std::string>{ "gcc@15.2.0" });
}

/**
 * A fault in the text comes back with where it is and which key it is for.
 */
TEST(GlobalConfigFileTest, ReturnsAFaultInTheText)
{
    const TempDirectory home;
    const auto file = home.writeFile("config.toml", "[defaults]\ntoolchain = 3\n");

    const auto read = readGlobalDefaultFile(file);

    ASSERT_FALSE(read.has_value());
    const auto* fault = std::get_if<GlobalConfigFault>(&read.error());
    ASSERT_NE(fault, nullptr);
    EXPECT_EQ(fault->line, 2U);
    EXPECT_EQ(fault->column, 13U);
    EXPECT_EQ(fault->key, "defaults.toolchain");
}

/**
 * A file that exists and cannot be read as a file is reported with its path.
 */
TEST(GlobalConfigFileTest, ReportsAFileThatCannotBeRead)
{
    const TempDirectory home;
    const auto file = home.makeDirectory("config.toml");

    const auto read = readGlobalDefaultFile(file);

    ASSERT_FALSE(read.has_value());
    const auto* failure = std::get_if<CannotReadConfig>(&read.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->path, file);
}

/**
 * Writing makes the directory the file belongs in.
 */
TEST(GlobalConfigFileTest, WritingCreatesTheDirectory)
{
    const TempDirectory home;
    const auto file = globalConfigFile(home.path() / "scrap" / "home");

    const auto written = writeGlobalDefaultFile(file, "gcc@15.2.0");

    ASSERT_TRUE(written.has_value());
    const auto read = readGlobalDefaultFile(file);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, std::optional<std::string>{ "gcc@15.2.0" });
}

/**
 * Writing nothing removes the default.
 */
TEST(GlobalConfigFileTest, WritingNothingRemovesTheDefault)
{
    const TempDirectory home;
    const auto file = home.writeFile("config.toml", "[defaults]\ntoolchain = \"system\"\n");

    const auto written = writeGlobalDefaultFile(file, std::nullopt);

    ASSERT_TRUE(written.has_value());
    const auto read = readGlobalDefaultFile(file);
    ASSERT_TRUE(read.has_value());
    EXPECT_FALSE(read->has_value());
}

/**
 * Text that cannot be read is left as it is.
 */
TEST(GlobalConfigFileTest, WritingLeavesTextThatCannotBeReadAlone)
{
    const TempDirectory home;
    const std::string broken = "[defaults\n";
    const auto file = home.writeFile("config.toml", broken);

    const auto written = writeGlobalDefaultFile(file, "system");

    ASSERT_FALSE(written.has_value());
    EXPECT_TRUE(std::holds_alternative<GlobalConfigFault>(written.error()));
    EXPECT_EQ(home.readFile(file), broken);
}

/**
 * A directory that cannot be made is reported with its path.
 */
TEST(GlobalConfigFileTest, ReportsADirectoryThatCannotBeCreated)
{
    const TempDirectory home;
    const auto blocker = home.writeFile("blocker", "");

    const auto written = writeGlobalDefaultFile(globalConfigFile(blocker), "system");

    ASSERT_FALSE(written.has_value());
    const auto* failure = std::get_if<CannotCreateDirectory>(&written.error());
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->path, blocker);
    EXPECT_TRUE(failure->code);
}
