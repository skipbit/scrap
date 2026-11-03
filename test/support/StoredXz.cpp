#include "support/StoredXz.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::TestSupport {

namespace {

constexpr std::size_t LargestChunk = 65536;
constexpr std::uint32_t Crc32Polynomial = 0xEDB88320U;

std::uint32_t crc32(const std::string_view bytes)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const char byte : bytes) {
        crc ^= static_cast<unsigned char>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ Crc32Polynomial) : (crc >> 1U);
        }
    }
    return ~crc;
}

std::string littleEndian(const std::uint32_t value)
{
    std::string bytes;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes += static_cast<char>((value >> shift) & 0xFFU);
    }
    return bytes;
}

std::string withCrc(std::string bytes)
{
    bytes += littleEndian(crc32(bytes));
    return bytes;
}

/** The variable-length number the xz format writes sizes in. */
std::string multibyte(std::size_t value)
{
    std::string bytes;
    while (value >= 0x80U) {
        bytes += static_cast<char>((value & 0x7FU) | 0x80U);
        value >>= 7U;
    }
    bytes += static_cast<char>(value);
    return bytes;
}

void padToFour(std::string& bytes)
{
    while ((bytes.size() % 4) != 0) {
        bytes += '\0';
    }
}

/** LZMA2 data that holds @p content in uncompressed chunks. */
std::string lzma2Chunks(const std::string_view content)
{
    std::string chunks;
    for (std::size_t offset = 0; offset < content.size(); offset += LargestChunk) {
        const std::string_view piece = content.substr(offset, LargestChunk);
        const std::size_t sizeMinusOne = piece.size() - 1;
        // The first chunk resets the dictionary; the rest continue it.
        chunks += (offset == 0) ? '\x01' : '\x02';
        chunks += static_cast<char>(sizeMinusOne >> 8U);
        chunks += static_cast<char>(sizeMinusOne & 0xFFU);
        chunks += piece;
    }
    chunks += '\0';
    return chunks;
}

/** A block header with one LZMA2 filter and no sizes, which the format allows. */
std::string blockHeader()
{
    // Header size, no flags beyond one filter, LZMA2, one property byte, 8 MiB dictionary.
    std::string header{ "\x02\x00\x21\x01\x16", 5 };
    header.resize(8, '\0');
    return withCrc(std::move(header));
}

}  // anonymous namespace

std::string storedXz(const std::string_view content)
{
    const std::string streamFlags{ "\x00\x01", 2 };  // CRC32 as the integrity check.

    std::string file{ "\xFD"
                      "7zXZ\0",
                      6 };
    file += streamFlags;
    file += littleEndian(crc32(streamFlags));

    const std::string header = blockHeader();
    const std::string data = lzma2Chunks(content);
    file += header;
    file += data;
    padToFour(file);
    file += littleEndian(crc32(content));

    std::string index{ "\x00\x01", 2 };
    index += multibyte(header.size() + data.size() + 4);
    index += multibyte(content.size());
    padToFour(index);
    index = withCrc(std::move(index));
    file += index;

    std::string footer = littleEndian(static_cast<std::uint32_t>((index.size() / 4) - 1));
    footer += streamFlags;
    file += littleEndian(crc32(footer));
    file += footer;
    file += "YZ";
    return file;
}

}  // namespace scrap::TestSupport
