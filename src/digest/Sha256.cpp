#include "digest/Sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace scrap::Digest {

namespace {

constexpr std::size_t ScheduleSize = 64;
constexpr std::size_t LengthFieldSize = 8;
constexpr unsigned BitsPerByte = 8;
constexpr unsigned BitsPerWord = 32;

constexpr std::array<std::uint32_t, ScheduleSize> RoundConstants{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::array<std::uint32_t, ScheduleSize> expandSchedule(const std::array<std::uint8_t, 64>& block)
{
    std::array<std::uint32_t, ScheduleSize> schedule{};
    for (std::size_t word = 0; word < block.size() / sizeof(std::uint32_t); ++word) {
        for (std::size_t byte = 0; byte < sizeof(std::uint32_t); ++byte) {
            schedule[word] = (schedule[word] << BitsPerByte) | block[(word * sizeof(std::uint32_t)) + byte];
        }
    }
    for (std::size_t round = 16; round < ScheduleSize; ++round) {
        const std::uint32_t early = schedule[round - 15];
        const std::uint32_t late = schedule[round - 2];
        const std::uint32_t sigma0 = std::rotr(early, 7) ^ std::rotr(early, 18) ^ (early >> 3U);
        const std::uint32_t sigma1 = std::rotr(late, 17) ^ std::rotr(late, 19) ^ (late >> 10U);
        schedule[round] = schedule[round - 16] + sigma0 + schedule[round - 7] + sigma1;
    }
    return schedule;
}

}  // anonymous namespace

void Sha256::compress(const std::array<std::uint8_t, BlockSize>& block)
{
    const auto schedule = expandSchedule(block);

    auto [a, b, c, d, e, f, g, h] = _state;
    for (std::size_t round = 0; round < ScheduleSize; ++round) {
        const std::uint32_t sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + sum1 + choice + RoundConstants[round] + schedule[round];
        const std::uint32_t sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + sum0 + majority;
    }

    const std::array<std::uint32_t, 8> mixed{ a, b, c, d, e, f, g, h };
    std::transform(_state.begin(), _state.end(), mixed.begin(), _state.begin(), [](const std::uint32_t left, const std::uint32_t right) {
        return left + right;
    });
}

void Sha256::update(std::string_view bytes)
{
    _totalSize += bytes.size();
    while (! bytes.empty()) {
        const std::size_t taken = std::min(bytes.size(), BlockSize - _pendingSize);
        std::transform(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(taken), _pending.begin() + static_cast<std::ptrdiff_t>(_pendingSize), [](const char byte) {
            return static_cast<std::uint8_t>(byte);
        });
        _pendingSize += taken;
        bytes.remove_prefix(taken);
        if (_pendingSize == BlockSize) {
            compress(_pending);
            _pendingSize = 0;
        }
    }
}

std::string Sha256::hexDigest() const
{
    // The end of the message is added to a copy, so that more may still follow.
    Sha256 finished = *this;
    const std::uint64_t totalBits = _totalSize * BitsPerByte;

    constexpr std::array<char, 1> EndMark{ static_cast<char>(0x80) };
    finished.update({ EndMark.data(), EndMark.size() });
    constexpr std::array<char, BlockSize> Zeros{};
    const std::size_t paddingSize = (BlockSize + BlockSize - LengthFieldSize - finished._pendingSize) % BlockSize;
    finished.update({ Zeros.data(), paddingSize });

    std::array<char, LengthFieldSize> lengthField{};
    for (std::size_t byte = 0; byte < LengthFieldSize; ++byte) {
        lengthField.at(byte) = static_cast<char>(totalBits >> ((LengthFieldSize - 1 - byte) * BitsPerByte));
    }
    finished.update({ lengthField.data(), lengthField.size() });

    constexpr std::string_view HexDigits = "0123456789abcdef";
    std::string digest;
    digest.reserve(finished._state.size() * sizeof(std::uint32_t) * 2);
    for (const std::uint32_t word : finished._state) {
        for (unsigned shift = BitsPerWord; shift > 0; shift -= 4) {
            digest.push_back(HexDigits.at((word >> (shift - 4)) & 0xfU));
        }
    }
    return digest;
}

}  // namespace scrap::Digest
