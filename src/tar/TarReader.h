#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace scrap::Tar {

/**
 * @brief The kinds of entry the reader accepts.
 */
enum class EntryType : std::uint8_t {
    File,
    Directory,
    SymbolicLink,
    HardLink
};

/**
 * @brief One entry of an archive, as the archive states it.
 *
 * Nothing here has been checked for safety: the path and the link target may
 * lead anywhere.
 */
struct Entry {
    std::string path;
    EntryType type = EntryType::File;
    std::string linkTarget;
    std::uint64_t size = 0;
    std::uint32_t mode = 0;
    std::int64_t modificationTime = 0;
};

/**
 * @brief Everything given so far has been read.
 */
struct NeedInput { };

/**
 * @brief The next bytes of the content of the last entry.
 *
 * The bytes stay valid until the next call to Reader::next().
 */
struct EntryData {
    std::string_view bytes;
};

/**
 * @brief The archive's end marker was read.
 */
struct EndOfArchive { };

using ReadEvent = std::variant<NeedInput, Entry, EntryData, EndOfArchive>;

/**
 * @brief The input ended before the archive's end marker.
 */
struct TruncatedArchive { };

/**
 * @brief The header at @p offset is not one the reader can make sense of: it
 *        fails its checksum, is neither ustar nor GNU, holds a malformed field,
 *        or is longer than any path could need.
 */
struct InvalidHeader {
    std::uint64_t offset = 0;
};

/**
 * @brief An entry of a kind the reader does not accept, such as a device or a
 *        sparse file.
 */
struct UnsupportedEntry {
    std::string path;
    char type = '\0';
};

using ReadError = std::variant<TruncatedArchive, InvalidHeader, UnsupportedEntry>;

/**
 * @brief Reads a tar archive from pieces given one at a time, without touching
 *        anything outside itself.
 *
 * It reads ustar and GNU archives, GNU long names and pax extended headers
 * (path, linkpath, size and mtime; a global pax header is skipped). Nothing in
 * the archive after the end marker is read.
 *
 * Give a piece, then call next() until it returns NeedInput, then give the next
 * piece. When the input is over, call endInput() and go on until EndOfArchive
 * or an error. After an error the reader is not used again.
 */
class Reader {
public:
    /**
     * @brief Give the next piece of the archive.
     *
     * The piece stays valid until next() returns NeedInput.
     */
    void give(std::string_view input);

    /**
     * @brief State that no more pieces follow.
     */
    void endInput();

    /**
     * @brief Read on to the next event.
     */
    [[nodiscard]] std::expected<ReadEvent, ReadError> next();

private:
    static constexpr std::size_t BlockSize = 512;

    enum class State : std::uint8_t {
        Header,
        Metadata,
        Data,
        Padding,
        Ended
    };

    enum class MetadataKind : std::uint8_t {
        LongName,
        LongLinkTarget,
        ExtendedHeader,
        GlobalExtendedHeader
    };

    /// What the headers before an entry say about it.
    struct Overrides {
        std::optional<std::string> path;
        std::optional<std::string> linkTarget;
        std::optional<std::uint64_t> size;
        std::optional<std::int64_t> modificationTime;
    };

    [[nodiscard]] static std::optional<MetadataKind> metadataKindOf(char type);

    std::string_view take(std::uint64_t size);
    [[nodiscard]] bool needsInput() const;
    void beginBody(std::uint64_t size, State state);

    [[nodiscard]] std::expected<std::optional<ReadEvent>, ReadError> readHeader();
    [[nodiscard]] std::expected<std::optional<ReadEvent>, ReadError> interpretBlock();
    [[nodiscard]] std::expected<std::optional<ReadEvent>, ReadError> readZeroBlock();
    [[nodiscard]] std::expected<void, ReadError> beginMetadata(MetadataKind kind, std::uint64_t size);
    [[nodiscard]] std::expected<Entry, ReadError> entryFromBlock(char type, std::uint64_t size);
    [[nodiscard]] std::expected<void, ReadError> readMetadata();
    [[nodiscard]] std::expected<void, ReadError> applyMetadata();
    [[nodiscard]] std::expected<void, ReadError> applyExtendedHeader();

    [[nodiscard]] InvalidHeader invalidHere() const
    {
        return InvalidHeader{ _headerOffset };
    }

    std::string_view _input;
    bool _inputEnded = false;
    std::uint64_t _offset = 0;
    std::uint64_t _headerOffset = 0;

    State _state = State::Header;
    std::array<char, BlockSize> _block{};
    std::size_t _blockFill = 0;
    bool _sawZeroBlock = false;

    MetadataKind _metadataKind = MetadataKind::LongName;
    std::string _metadata;
    Overrides _overrides;

    std::uint64_t _bodyRemaining = 0;
    std::uint64_t _paddingRemaining = 0;
};

}  // namespace scrap::Tar
