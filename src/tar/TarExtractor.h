#pragma once

#include "tar/Descriptor.h"
#include "tar/TarReader.h"

#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace scrap::Tar {

/**
 * @brief A member's path is empty or absolute, holds "..", or goes through
 *        something that is not a directory, a symbolic link among them.
 */
struct UnsafeMemberPath {
    std::string path;
};

/**
 * @brief A link would lead outside the directory, or a hard link names something
 *        other than a file.
 */
struct UnsafeLinkTarget {
    std::string path;
    std::string target;
};

/**
 * @brief The operating system refused to create or write a member. file_exists
 *        means the path is already taken.
 */
struct CannotWriteMember {
    std::string path;
    std::error_code code;
};

using ExtractError = std::variant<ReadError, UnsafeMemberPath, UnsafeLinkTarget, CannotWriteMember>;

/**
 * @brief Extracts a tar archive given piece by piece into a directory, and keeps
 *        everything it creates inside that directory.
 *
 * Paths are walked from the directory one component at a time without following
 * symbolic links, so nothing is written through one, including one that was in
 * the directory beforehand. Nothing is overwritten: a member whose path is taken
 * is an error.
 *
 * Files keep their permission bits and modification time, under the process
 * umask. Set-user-ID, set-group-ID and sticky bits are dropped. Directories
 * always stay usable by their owner, so that what was extracted can be removed
 * again. A directory the archive only implies takes what umask leaves of 0777.
 *
 * After an error what was extracted so far stays in place, for the caller to
 * discard; the extractor is not used again.
 */
class Extractor {
public:
    /**
     * @brief Begin extracting into the existing @p directory.
     *
     * @return The extractor, or what the operating system reported.
     */
    [[nodiscard]] static std::expected<Extractor, ExtractError> into(const std::filesystem::path& directory);

    /**
     * @brief Extract what the next piece of the archive completes.
     */
    [[nodiscard]] std::expected<void, ExtractError> feed(std::string_view piece);

    /**
     * @brief State that the archive is over.
     *
     * @return Nothing when the archive ended with its end marker and every
     *         member was written, or what went wrong.
     */
    [[nodiscard]] std::expected<void, ExtractError> finish();

private:
    explicit Extractor(Descriptor root);

    [[nodiscard]] std::expected<void, ExtractError> drain();
    [[nodiscard]] std::expected<void, ExtractError> begin(const Entry& entry);
    [[nodiscard]] std::expected<void, ExtractError> makeDirectory(const Entry& entry, std::span<const std::string_view> components) const;
    [[nodiscard]] std::expected<void, ExtractError> makeSymbolicLink(const Entry& entry, std::span<const std::string_view> components) const;
    [[nodiscard]] std::expected<void, ExtractError> makeHardLink(const Entry& entry, std::span<const std::string_view> components) const;
    [[nodiscard]] std::expected<void, ExtractError> beginFile(const Entry& entry, std::span<const std::string_view> components);
    [[nodiscard]] std::expected<void, ExtractError> writeContent(std::string_view bytes);
    [[nodiscard]] std::expected<void, ExtractError> endFile();

    Descriptor _root;
    Reader _reader;
    Descriptor _file;
    std::string _filePath;
    std::int64_t _fileModificationTime = 0;
};

}  // namespace scrap::Tar
