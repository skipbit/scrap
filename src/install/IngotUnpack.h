#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <string>
#include <system_error>

namespace scrap::Install {

/**
 * @brief The ingot could not be read or unpacked, or holds no usable toolchain.
 */
struct IngotNotUnpacked {
    std::string reason;
    std::error_code code;  ///< What the operating system reported, when it reported anything.
};

/**
 * @brief Decode the xz file @p ingot and extract the archive in it into the
 *        existing directory @p directory.
 *
 * @return Nothing on success, or why it could not be unpacked.
 */
[[nodiscard]] std::expected<void, IngotNotUnpacked> unpack(const std::filesystem::path& ingot, const std::filesystem::path& directory);

}  // namespace scrap::Install
