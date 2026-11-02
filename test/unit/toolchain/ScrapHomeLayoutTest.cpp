#include "toolchain/ScrapHomeLayout.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace scrap::Toolchain;

/**
 * Toolchains are kept in a toolchains directory below the home.
 */
TEST(ScrapHomeLayoutTest, NamesTheToolchainsDirectory)
{
    EXPECT_EQ(toolchainsDirectory("/opt/scrap"), std::filesystem::path{ "/opt/scrap/toolchains" });
}

/**
 * The versions of a family are kept in a directory of its name below the
 * toolchains directory.
 */
TEST(ScrapHomeLayoutTest, NamesTheDirectoryOfAFamily)
{
    EXPECT_EQ(familyDirectory("/opt/scrap", "gcc"), std::filesystem::path{ "/opt/scrap/toolchains/gcc" });
}

/**
 * The global default is kept in config.toml directly below the home.
 */
TEST(ScrapHomeLayoutTest, NamesConfigTomlBelowTheHome)
{
    EXPECT_EQ(globalConfigFile("/opt/scrap"), std::filesystem::path{ "/opt/scrap/config.toml" });
}
