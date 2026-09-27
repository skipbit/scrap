#include "build/Parallelism.h"

#if defined(__linux__)
#include <cerrno>
#include <cstddef>
#include <memory>
#include <sched.h>
#else
#include <thread>
#endif

namespace scrap::Build {

#if defined(__linux__)

namespace {

/// The most processors a set is made for. Linux supports far fewer.
constexpr int MostProcessorsAsked = 65536;

/// Frees a set made by CPU_ALLOC().
struct CpuSetFree {
    void operator()(cpu_set_t* set) const
    {
        CPU_FREE(set);
    }
};

}  // anonymous namespace

unsigned availableParallelism()
{
    // A fixed cpu_set_t holds 1024 processors, and the system refuses a set
    // smaller than the processors it has, so a larger set is tried until
    // one holds them.
    for (int processors = CPU_SETSIZE; processors <= MostProcessorsAsked; processors *= 2) {
        const std::unique_ptr<cpu_set_t, CpuSetFree> allowed{ CPU_ALLOC(processors) };
        if (! allowed) {
            return 1;
        }
        const std::size_t size = CPU_ALLOC_SIZE(processors);
        CPU_ZERO_S(size, allowed.get());
        if (sched_getaffinity(0, size, allowed.get()) == 0) {
            const int count = CPU_COUNT_S(size, allowed.get());
            return (count > 0) ? static_cast<unsigned>(count) : 1;
        }
        if (errno != EINVAL) {
            return 1;
        }
    }
    return 1;
}

#else

unsigned availableParallelism()
{
    const unsigned count = std::thread::hardware_concurrency();
    return (count > 0) ? count : 1;
}

#endif

}  // namespace scrap::Build
