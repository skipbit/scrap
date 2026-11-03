#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <functional>
#include <string_view>
#include <system_error>
#include <variant>

namespace scrap::Xz {

/**
 * @brief The file could not be read.
 */
struct CannotReadFile {
    std::filesystem::path path;
    std::error_code code;
};

/**
 * @brief The file does not begin as an xz file.
 */
struct NotAnXzFile {
    std::filesystem::path path;
};

/**
 * @brief The data does not decode, or does not match its integrity check.
 */
struct CorruptData {
    std::filesystem::path path;
};

/**
 * @brief The file ended before its data did.
 */
struct TruncatedData {
    std::filesystem::path path;
};

/**
 * @brief Memory for decoding could not be obtained.
 */
struct OutOfMemory {
    std::filesystem::path path;
};

/**
 * @brief The consumer asked to stop.
 */
struct ConsumerStopped { };

using DecodeError = std::variant<CannotReadFile, NotAnXzFile, CorruptData, TruncatedData, OutOfMemory, ConsumerStopped>;

/**
 * @brief Decode the xz file at @p file, handing the decoded bytes on piece by
 *        piece.
 *
 * Blocks are decoded on as many threads as the machine has CPUs while the
 * memory they take stays under a quarter of the machine's. A file of one block
 * decodes on one thread.
 *
 * Bytes handed on before a failure were decoded from data whose integrity check
 * has not been reached, so a consumer discards them when this fails.
 *
 * @param consume Takes the next piece. Returns false to stop.
 * @return Nothing when all of the file decoded and was consumed, or what went
 *         wrong.
 */
[[nodiscard]] std::expected<void, DecodeError> decodeFile(const std::filesystem::path& file, const std::function<bool(std::string_view)>& consume);

}  // namespace scrap::Xz
