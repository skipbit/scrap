#pragma once

#include "command/RuntimeEnvironment.h"

#include <string>

namespace scrap::Command {

/**
 * @brief The URL of the toolchain index: SCRAP_TOOLCHAIN_INDEX when it is
 *        set, the official channel's index otherwise.
 */
[[nodiscard]] std::string toolchainIndexUrl(const RuntimeEnvironment& env);

}  // namespace scrap::Command
