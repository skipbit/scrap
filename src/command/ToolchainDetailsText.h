#pragma once

#include "command/ToolchainDetails.h"

#include <optional>
#include <string>

namespace scrap::Command {

/**
 * @brief @p details as lines of "label: value", the values lined up.
 *
 * What an install record does not hold is told as not recorded. The
 * paths inside an installed toolchain are told in full.
 */
[[nodiscard]] std::string renderToolchainDetails(const ToolchainDetails& details);

/**
 * @brief The hint to reinstall a toolchain whose record lacks where it came
 *        from or when it was installed, or nothing when it holds both.
 */
[[nodiscard]] std::optional<std::string> renderUnrecordedHint(const InstalledDetails& details);

}  // namespace scrap::Command
