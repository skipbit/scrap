#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace scrap::Digest {

/**
 * @brief SHA-256 of bytes given piece by piece.
 */
class Sha256 {
public:
    /**
     * @brief Add @p bytes to what has been given so far.
     */
    void update(std::string_view bytes);

    /**
     * @brief The digest of what has been given so far, in lowercase hexadecimal.
     *
     * More bytes may be given afterwards.
     */
    [[nodiscard]] std::string hexDigest() const;

private:
    static constexpr std::size_t BlockSize = 64;

    void compress(const std::array<std::uint8_t, BlockSize>& block);

    std::array<std::uint32_t, 8> _state{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    std::array<std::uint8_t, BlockSize> _pending{};
    std::size_t _pendingSize = 0;
    std::uint64_t _totalSize = 0;
};

}  // namespace scrap::Digest
