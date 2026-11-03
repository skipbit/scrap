#pragma once

#include "fetch/UrlFetch.h"
#include "toolchain/HostEnvironment.h"
#include "toolchain/IngotSelection.h"

#include <chrono>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace scrap::Install {

/**
 * @brief The ingot could not be fetched.
 */
struct IngotNotFetched {
    Fetch::FetchError error;
};

/**
 * @brief What was fetched is not what the index promised.
 */
struct IngotDoesNotMatch { };

/**
 * @brief The ingot is kept in a registry, which this version cannot fetch from.
 */
struct RegistryNotSupported { };

/**
 * @brief The ingot could not be unpacked, or holds no usable toolchain.
 */
struct IngotNotUnpacked {
    std::string reason;
    std::error_code code;  ///< What the operating system reported, when it reported anything.
};

/**
 * @brief The toolchain could not be put where it belongs.
 */
struct IngotNotPlaced {
    std::filesystem::path path;
    std::error_code code;
};

using IngotInstallFailure = std::variant<IngotNotFetched, IngotDoesNotMatch, RegistryNotSupported, IngotNotUnpacked, IngotNotPlaced>;

/**
 * @brief What to install, and where.
 */
struct IngotInstallRequest {
    std::filesystem::path scrapHome;
    Toolchain::SelectedIngot selected;
    Toolchain::HostEnvironment environment;
    std::optional<std::filesystem::path> certificateBundle;
    std::function<void(std::string_view)> announce;  ///< Takes each step as it begins.
};

/**
 * @brief Fetch an ingot, check it against the index and put the toolchain in
 *        place, in place of what is there.
 *
 * Everything happens in a directory of its own below SCRAP_HOME until the
 * finished toolchain is renamed into place, so a failure at any step leaves what
 * was installed before as it was and nothing new. A directory left there by a
 * run that was stopped is removed by a later run once it is a day old.
 *
 * @return The directory of the toolchain, or what went wrong.
 */
[[nodiscard]] std::expected<std::filesystem::path, IngotInstallFailure> installIngot(const IngotInstallRequest& request);

/**
 * @brief Remove what stopped runs left in @p workDirectory, when older than
 *        @p now by a day.
 *
 * A younger one may belong to a run still going. Failures are passed over: this
 * only reclaims space.
 */
void discardAbandonedWork(const std::filesystem::path& workDirectory, std::filesystem::file_time_type now);

}  // namespace scrap::Install
