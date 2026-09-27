#pragma once

namespace scrap::Build {

/**
 * @brief How many steps of a build run at once when nothing says otherwise.
 *
 * On Linux this is the number of processors the calling thread may run on,
 * so a build started under taskset or in a container limited to some of the
 * processors keeps to them; a quota on processor time is not looked at. On
 * other systems it is the number of processors. A count that cannot be found
 * is taken as 1.
 *
 * @return At least 1.
 */
[[nodiscard]] unsigned availableParallelism();

}  // namespace scrap::Build
