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
 * @brief Where the toolchain @p family @p version for the environment named
 *        @p environment is kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path toolchainDirectory(const std::filesystem::path& scrapHome, std::string_view family, std::string_view version, std::string_view environment);

/**
 * @brief Where the copy of the toolchain index is kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path indexCopyFile(const std::filesystem::path& scrapHome);

/**
 * @brief Where the URL that copy was fetched from is kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path indexCopySourceFile(const std::filesystem::path& scrapHome);

/**
 * @brief Where work under way is kept below @p scrapHome. Nothing there lasts.
 *        It is inside the toolchains directory so that finished work can be
 *        renamed into place wherever that directory leads.
 */
[[nodiscard]] std::filesystem::path temporaryDirectory(const std::filesystem::path& scrapHome);

/**
 * @brief Where the global default toolchain is kept below @p scrapHome.
 */
[[nodiscard]] std::filesystem::path globalConfigFile(const std::filesystem::path& scrapHome);

}  // namespace scrap::Toolchain
