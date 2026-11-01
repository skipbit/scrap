#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief The platform and architecture a toolchain is built for, spelled as
 *        the index spells them.
 */
struct HostEnvironment {
    std::string_view platform;  ///< e.g. linux, darwin
    std::string_view arch;      ///< e.g. x86_64, aarch64

    /** The name of the directory a toolchain for it is kept in, e.g. linux-x86_64. */
    [[nodiscard]] std::string directoryName() const;
};

/**
 * @brief The environment this scrap was built for, which is the host it runs
 *        on, or nothing when no toolchain is built for one like it.
 */
[[nodiscard]] std::optional<HostEnvironment> hostEnvironment();

}  // namespace scrap::Toolchain
