#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace scrap::TestTar {

/**
 * @brief The fields of a header block a test may want to set.
 */
struct HeaderFields {
    std::string_view name{};
    char type = '0';
    std::uint64_t size = 0;
    std::string_view linkName{};
    std::uint32_t mode = 0644;
    std::uint64_t modificationTime = 0;
    std::string_view prefix{};
    bool gnu = false;
};

/**
 * @brief One header block with a correct checksum.
 */
[[nodiscard]] std::string headerBlock(const HeaderFields& fields);

/**
 * @brief @p block with its checksum worked out again, for a test that alters
 *        other fields of a header.
 */
[[nodiscard]] std::string withChecksum(std::string block);

/**
 * @brief @p content followed by zeros up to the next block boundary.
 */
[[nodiscard]] std::string padded(std::string_view content);

/**
 * @brief The two zero blocks that end an archive.
 */
[[nodiscard]] std::string endMarker();

/**
 * @brief A regular file with its content.
 */
[[nodiscard]] std::string fileEntry(std::string_view name, std::string_view content, std::uint32_t mode = 0644, std::uint64_t modificationTime = 0);

/**
 * @brief A directory.
 */
[[nodiscard]] std::string directoryEntry(std::string_view name, std::uint32_t mode = 0755);

/**
 * @brief A symbolic link.
 */
[[nodiscard]] std::string symbolicLinkEntry(std::string_view name, std::string_view target);

/**
 * @brief A hard link.
 */
[[nodiscard]] std::string hardLinkEntry(std::string_view name, std::string_view target);

/**
 * @brief A GNU long name (type L) or long link target (type K).
 */
[[nodiscard]] std::string gnuLongEntry(char type, std::string_view text);

/**
 * @brief A pax extended header (type x, or g for a global one) with the body
 *        @p records.
 */
[[nodiscard]] std::string paxEntry(std::string_view records, char type = 'x');

/**
 * @brief One record of a pax extended header, with its length worked out.
 */
[[nodiscard]] std::string paxRecord(std::string_view key, std::string_view value);

}  // namespace scrap::TestTar
