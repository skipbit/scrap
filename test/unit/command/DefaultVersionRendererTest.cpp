#include <gtest/gtest.h>

#include "command/DefaultVersionRenderer.h"

using namespace scrap::Command;

/**
 * Version string should be non-empty and contain the application name.
 */
TEST(DefaultVersionRendererTest, ReturnsVersionString)
{
    DefaultVersionRenderer renderer;
    auto result = renderer.render();

    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("scrap"), std::string::npos);
}

/**
 * Version string should contain a version number pattern.
 */
TEST(DefaultVersionRendererTest, ContainsVersionNumber)
{
    DefaultVersionRenderer renderer;
    auto result = renderer.render();

    // Expect at least one '.' separating version components (e.g. "0.0.1").
    EXPECT_NE(result.find('.'), std::string::npos);
}
