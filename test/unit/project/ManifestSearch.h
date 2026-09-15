#pragma once

#include <filesystem>
#include <optional>

namespace scrap::TestSupport {

/**
 * @brief The first directory at or above @p directory that holds a manifest.
 *
 * The "no project anywhere above" tests depend on the machine's temp location
 * not sitting inside a scrap project. Checking it makes an unusual machine
 * skip the test, naming the directory responsible, instead of reporting a
 * failure that is not about the code.
 *
 * The predicate has to be the one findProjectRoot uses. Testing existence
 * instead would treat a *directory* named scrap.toml as a project and skip
 * the very test that exists to show it is not one.
 *
 * @param directory Directory to start at.
 * @return The directory holding the manifest, or nothing.
 */
[[nodiscard]] std::optional<std::filesystem::path> manifestAbove(const std::filesystem::path& directory);

}  // namespace scrap::TestSupport
