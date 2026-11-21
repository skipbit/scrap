#include "command/ProjectBuild.h"

#include "command/ParsedOptions.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

using namespace scrap::Command;

namespace {

/**
 * Options that give @p jobs to the jobs option.
 */
ParsedOptions withJobs(const std::int64_t jobs)
{
    ParsedOptions options;
    options.named.emplace(std::string{ JobsOption }, jobs);
    return options;
}

}  // namespace

TEST(ProjectBuildTest, RunsAsManyStepsAtOnceAsTheJobsAsk)
{
    EXPECT_EQ(requestedJobs(withJobs(1)), std::optional<unsigned>{ 1U });
    EXPECT_EQ(requestedJobs(withJobs(3)), std::optional<unsigned>{ 3U });
}

/**
 * More steps than unsigned holds never run at once.
 */
TEST(ProjectBuildTest, RunsAtMostAsManyStepsAsUnsignedHolds)
{
    EXPECT_EQ(requestedJobs(withJobs(std::numeric_limits<std::int64_t>::max())), std::optional<unsigned>{ std::numeric_limits<unsigned>::max() });
}

TEST(ProjectBuildTest, AsksForNoNumberOfJobsWithoutTheOption)
{
    EXPECT_EQ(requestedJobs(ParsedOptions{}), std::nullopt);
}
