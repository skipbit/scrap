#pragma once

#include "toolchain/IngotSelection.h"

#include <string>

namespace scrap::Command {

/**
 * @brief Why a toolchain the index holds cannot be installed on the host,
 *        as a few words to follow a label or fill a column.
 */
[[nodiscard]] std::string renderInstallBarrier(const Toolchain::IngotSelectionFault& barrier);

}  // namespace scrap::Command
