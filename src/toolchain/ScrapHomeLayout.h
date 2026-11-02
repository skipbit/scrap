#pragma once

#include <filesystem>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief Where the toolchains are kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path toolchainsDirectory(const std::filesystem::path& scrapHome);

/**
 * @brief Where the versions of @p family are kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path familyDirectory(const std::filesystem::path& scrapHome, std::string_view family);

/**
 * @brief Where the global default toolchain is kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path globalConfigFile(const std::filesystem::path& scrapHome);

}  // namespace scrap::Toolchain
