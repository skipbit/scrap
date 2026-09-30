#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

namespace scrap::Compile {

/**
 * @brief What a build is for, which decides what the compiler is asked for
 *        and where the build writes.
 */
enum class BuildProfile : std::uint8_t {
    Debug,   ///< For debugging: debugging information, no optimisation.
    Release  ///< For use: optimised, without debugging information or assertions.
};

/**
 * @brief The directory, relative to the project root, a build of @p profile
 *        writes to.
 */
[[nodiscard]] constexpr std::string_view buildDirectoryOf(const BuildProfile profile)
{
    switch (profile) {
    case BuildProfile::Debug:
        return "build/debug";
    case BuildProfile::Release:
        return "build/release";
    }
    std::unreachable();
}

/**
 * @brief The name the output gives @p profile.
 */
[[nodiscard]] constexpr std::string_view profileName(const BuildProfile profile)
{
    switch (profile) {
    case BuildProfile::Debug:
        return "debug";
    case BuildProfile::Release:
        return "release";
    }
    std::unreachable();
}

}  // namespace scrap::Compile
