#include "build/Parallelism.h"

#include <gtest/gtest.h>

#if defined(__linux__)
#include <sched.h>
#endif

using namespace scrap::Build;

/**
 * Some number of steps always runs.
 */
TEST(ParallelismTest, RunsAtLeastOneStep)
{
    EXPECT_GE(availableParallelism(), 1U);
}

#if defined(__linux__)

namespace {

/**
 * Keeps the calling thread on the first @p count processors it may run on,
 * and gives it back the processors it had once it goes.
 */
// NOLINTBEGIN(misc-include-cleaner) - cpu_set_t, the CPU_ macros and the affinity calls are provided by <sched.h>
class RestrictedAffinity {
public:
    explicit RestrictedAffinity(int count)
    {
        CPU_ZERO(&_previous);
        _held = (sched_getaffinity(0, sizeof(_previous), &_previous) == 0);
        cpu_set_t restricted;
        CPU_ZERO(&restricted);
        int kept = 0;
        for (int cpu = 0; (cpu < CPU_SETSIZE) && (kept < count); ++cpu) {
            if (CPU_ISSET(cpu, &_previous)) {
                CPU_SET(cpu, &restricted);
                ++kept;
            }
        }
        _restricted = _held && (kept == count) && (sched_setaffinity(0, sizeof(restricted), &restricted) == 0);
    }

    ~RestrictedAffinity()
    {
        if (_restricted) {
            (void)sched_setaffinity(0, sizeof(_previous), &_previous);
        }
    }

    RestrictedAffinity(const RestrictedAffinity&) = delete;
    RestrictedAffinity& operator=(const RestrictedAffinity&) = delete;
    RestrictedAffinity(RestrictedAffinity&&) = delete;
    RestrictedAffinity& operator=(RestrictedAffinity&&) = delete;

    [[nodiscard]] bool restricted() const
    {
        return _restricted;
    }

private:
    cpu_set_t _previous;
    bool _held = false;
    bool _restricted = false;
};
// NOLINTEND(misc-include-cleaner)

}  // namespace

/**
 * A thread kept to one processor runs one step at a time.
 */
TEST(ParallelismTest, KeepsToOneProcessorWhenLimitedToIt)
{
    const RestrictedAffinity limit{ 1 };
    ASSERT_TRUE(limit.restricted()) << "the processors of this thread could not be limited";

    EXPECT_EQ(availableParallelism(), 1U);
}

/**
 * A thread kept to two processors runs two steps at once.
 */
TEST(ParallelismTest, CountsTheProcessorsItMayRunOn)
{
    const RestrictedAffinity limit{ 2 };
    if (! limit.restricted()) {
        GTEST_SKIP() << "this thread may run on fewer than two processors";
    }

    EXPECT_EQ(availableParallelism(), 2U);
}

#endif
