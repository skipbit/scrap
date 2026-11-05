#pragma once

#include "fetch/UrlFetch.h"
#include "install/IndexCopy.h"
#include "install/IngotInstaller.h"
#include "toolchain/IngotSelection.h"

#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief Report why the index offers nothing to install for @p specifier.
 */
[[nodiscard]] std::string renderSelectionFault(std::string_view specifier, const Toolchain::IngotSelectionFault& fault);

/**
 * @brief Report why there is no index to choose from.
 */
[[nodiscard]] std::string renderIndexFault(const Install::IndexFault& fault);

/**
 * @brief Report why installing @p specifier failed.
 */
[[nodiscard]] std::string renderInstallFailure(std::string_view specifier, const Install::IngotInstallFailure& failure);

}  // namespace scrap::Command
