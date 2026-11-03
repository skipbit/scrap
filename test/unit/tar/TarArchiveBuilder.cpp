#include "unit/tar/TarArchiveBuilder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::TestTar {

namespace {

constexpr std::size_t BlockSize = 512;
constexpr std::size_t ChecksumOffset = 148;
constexpr std::size_t ChecksumSize = 8;

void put(std::string& block, const std::size_t offset, std::string_view text)
{
    std::copy(text.begin(), text.end(), block.begin() + static_cast<std::ptrdiff_t>(offset));
}

void putOctal(std::string& block, const std::size_t offset, const std::size_t size, const std::uint64_t value)
{
    std::string text(size - 1, '0');
    std::uint64_t remaining = value;
    for (auto digit = text.rbegin(); digit != text.rend(); ++digit) {
        *digit = static_cast<char>('0' + (remaining & 7U));
        remaining >>= 3U;
    }
    put(block, offset, text);
}

}  // anonymous namespace

std::string headerBlock(const HeaderFields& fields)
{
    std::string block(BlockSize, '\0');
    put(block, 0, fields.name);
    putOctal(block, 100, 8, fields.mode);
    putOctal(block, 108, 8, 0);
    putOctal(block, 116, 8, 0);
    putOctal(block, 124, 12, fields.size);
    putOctal(block, 136, 12, fields.modificationTime);
    block[156] = fields.type;
    put(block, 157, fields.linkName);
    if (fields.gnu) {
        put(block, 257, std::string_view{ "ustar  \0", 8 });
    } else {
        put(block, 257, std::string_view{ "ustar\0"
                                          "00",
                                          8 });
        put(block, 345, fields.prefix);
    }

    return withChecksum(std::move(block));
}

std::string withChecksum(std::string block)
{
    put(block, ChecksumOffset, std::string(ChecksumSize, ' '));
    const std::uint64_t sum = std::accumulate(block.begin(), block.end(), std::uint64_t{ 0 }, [](const std::uint64_t total, const char byte) {
        return total + static_cast<unsigned char>(byte);
    });
    putOctal(block, ChecksumOffset, ChecksumSize - 1, sum);
    return block;
}

std::string padded(std::string_view content)
{
    std::string body{ content };
    body.resize((content.size() + BlockSize - 1) / BlockSize * BlockSize, '\0');
    return body;
}

std::string endMarker()
{
    return std::string(BlockSize * 2, '\0');
}

std::string fileEntry(std::string_view name, std::string_view content, const std::uint32_t mode, const std::uint64_t modificationTime)
{
    return headerBlock({ .name = name, .size = content.size(), .mode = mode, .modificationTime = modificationTime }) + padded(content);
}

std::string directoryEntry(std::string_view name, const std::uint32_t mode)
{
    return headerBlock({ .name = name, .type = '5', .mode = mode });
}

std::string symbolicLinkEntry(std::string_view name, std::string_view target)
{
    return headerBlock({ .name = name, .type = '2', .linkName = target, .mode = 0777 });
}

std::string hardLinkEntry(std::string_view name, std::string_view target)
{
    return headerBlock({ .name = name, .type = '1', .linkName = target });
}

std::string gnuLongEntry(const char type, std::string_view text)
{
    std::string body{ text };
    body.push_back('\0');
    return headerBlock({ .name = "././@LongLink", .type = type, .size = body.size(), .gnu = true }) + padded(body);
}

std::string paxEntry(std::string_view records, const char type)
{
    return headerBlock({ .name = "PaxHeaders/entry", .type = type, .size = records.size() }) + padded(records);
}

std::string paxRecord(std::string_view key, std::string_view value)
{
    // The length counts its own digits, so it is worked out until it settles.
    const std::size_t rest = key.size() + value.size() + 3;
    std::size_t length = rest;
    while (length != rest + std::to_string(length).size()) {
        length = rest + std::to_string(length).size();
    }
    return std::to_string(length) + " " + std::string{ key } + "=" + std::string{ value } + "\n";
}

}  // namespace scrap::TestTar
