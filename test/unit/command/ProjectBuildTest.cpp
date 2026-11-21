#include "command/ProjectBuild.h"

#include "build/Parallelism.h"
#include "command/ParsedOptions.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
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
    EXPECT_EQ(requestedParallelism(withJobs(1)), 1U);
    EXPECT_EQ(requestedParallelism(withJobs(3)), 3U);
}

/**
 * More steps than unsigned holds never run at once.
 */
TEST(ProjectBuildTest, RunsAtMostAsManyStepsAsUnsignedHolds)
{
    EXPECT_EQ(requestedParallelism(withJobs(std::numeric_limits<std::int64_t>::max())), std::numeric_limits<unsigned>::max());
}

TEST(ProjectBuildTest, RunsAsManyStepsAsTheSystemHasProcessorsForWithoutJobs)
{
    EXPECT_EQ(requestedParallelism(ParsedOptions{}), scrap::Build::availableParallelism());
}
