#include "build/Parallelism.h"

#if defined(__linux__)
#include <sched.h>
#else
#include <thread>
#endif

namespace scrap::Build {

unsigned availableParallelism()
{
#if defined(__linux__)
    // NOLINTBEGIN(misc-include-cleaner) - cpu_set_t, CPU_ZERO, CPU_COUNT and sched_getaffinity() are provided by <sched.h>
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    const int count = (sched_getaffinity(0, sizeof(allowed), &allowed) == 0) ? CPU_COUNT(&allowed) : 0;
    // NOLINTEND(misc-include-cleaner)
    return (count > 0) ? static_cast<unsigned>(count) : 1;
#else
    const unsigned count = std::thread::hardware_concurrency();
    return (count > 0) ? count : 1;
#endif
}

}  // namespace scrap::Build
