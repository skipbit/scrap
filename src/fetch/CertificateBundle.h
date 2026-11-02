#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace scrap::Fetch {

/**
 * @brief The places operating systems keep their root certificates in one file.
 */
[[nodiscard]] std::span<const std::string_view> knownCertificateBundles();

/**
 * @brief The file of root certificates to verify a host against.
 *
 * @param requested The file the user named, used as it is so that a wrong name
 *                  is reported rather than passed over. Empty names none.
 * @param known     The places to look when the user named none, in order.
 * @return The file, or nothing when the user named none and no known place
 *         holds a regular file.
 */
[[nodiscard]] std::optional<std::filesystem::path> findCertificateBundle(std::string_view requested, std::span<const std::string_view> known);

}  // namespace scrap::Fetch
