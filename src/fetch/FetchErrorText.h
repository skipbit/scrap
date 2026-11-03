#pragma once

#include "fetch/UrlFetch.h"

#include <string>

namespace scrap::Fetch {

/**
 * @brief @p error in words, to follow "cannot ...:" in a report.
 */
[[nodiscard]] std::string describeFetchError(const FetchError& error);

}  // namespace scrap::Fetch
