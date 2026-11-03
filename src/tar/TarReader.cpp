#include "tar/TarReader.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace scrap::Tar {

namespace {

// Where the fields of a header block are: { offset, size }.
struct Field {
    std::size_t offset;
    std::size_t size;
};
constexpr Field NameField{ 0, 100 };
constexpr Field ModeField{ 100, 8 };
constexpr Field SizeField{ 124, 12 };
constexpr Field TimeField{ 136, 12 };
constexpr Field ChecksumField{ 148, 8 };
constexpr std::size_t TypeOffset = 156;
constexpr Field LinkField{ 157, 100 };
constexpr Field MagicField{ 257, 6 };
constexpr Field PrefixField{ 345, 155 };

constexpr std::string_view PosixMagic{ "ustar\0", 6 };
constexpr std::string_view GnuMagic{ "ustar ", 6 };

/// A path of any host fits in far less; a header past this is not a real one.
constexpr std::uint64_t MaxMetadataSize = std::uint64_t{ 1 } << 20U;

constexpr unsigned BitsPerOctalDigit = 3;
constexpr unsigned BitsPerByte = 8;
constexpr unsigned char BaseTwoFiftySixMark = 0x80;
constexpr unsigned char NegativeNumberMark = 0x40;

constexpr std::uint64_t LargestNumber = std::numeric_limits<std::uint64_t>::max();

std::string_view fieldOf(std::string_view block, const Field field)
{
    return block.substr(field.offset, field.size);
}

std::string_view untilNul(std::string_view field)
{
    return field.substr(0, field.find('\0'));
}

std::optional<std::uint64_t> parseBaseTwoFiftySix(std::string_view field)
{
    const auto first = static_cast<unsigned char>(field.front());
    if ((first & NegativeNumberMark) != 0) {
        return std::nullopt;
    }
    std::uint64_t value = first & static_cast<unsigned char>(~BaseTwoFiftySixMark);
    for (const char byte : field.substr(1)) {
        if (value > (LargestNumber >> BitsPerByte)) {
            return std::nullopt;
        }
        value = (value << BitsPerByte) | static_cast<unsigned char>(byte);
    }
    return value;
}

std::optional<std::uint64_t> parseOctal(std::string_view field)
{
    const std::size_t start = field.find_first_not_of(' ');
    if (start == std::string_view::npos) {
        return 0;
    }
    field.remove_prefix(start);

    std::uint64_t value = 0;
    while (! field.empty() && field.front() >= '0' && field.front() <= '7') {
        if (value > (LargestNumber >> BitsPerOctalDigit)) {
            return std::nullopt;
        }
        value = (value << BitsPerOctalDigit) | static_cast<std::uint64_t>(field.front() - '0');
        field.remove_prefix(1);
    }

    // Only the terminators may follow the digits.
    if (field.find_first_not_of(std::string_view{ " \0", 2 }) != std::string_view::npos) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint64_t> parseNumber(std::string_view field)
{
    if ((static_cast<unsigned char>(field.front()) & BaseTwoFiftySixMark) != 0) {
        return parseBaseTwoFiftySix(field);
    }
    return parseOctal(field);
}

std::optional<std::uint64_t> parseDecimal(std::string_view text)
{
    std::uint64_t value = 0;
    const char* const end = text.data() + text.size();
    const auto [stopped, failure] = std::from_chars(text.data(), end, value);
    if (failure != std::errc{} || stopped != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::int64_t> toTime(const std::uint64_t seconds)
{
    if (seconds > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(seconds);
}

bool checksumMatches(std::string_view block)
{
    const auto stated = parseOctal(fieldOf(block, ChecksumField));
    if (! stated) {
        return false;
    }
    const auto sumOf = [](std::string_view bytes) {
        return std::accumulate(bytes.begin(), bytes.end(), std::uint64_t{ 0 }, [](const std::uint64_t sum, const char byte) {
            return sum + static_cast<unsigned char>(byte);
        });
    };
    // The checksum is taken with its own field read as spaces.
    const std::uint64_t sum = sumOf(block.substr(0, ChecksumField.offset)) + (ChecksumField.size * ' ') + sumOf(block.substr(ChecksumField.offset + ChecksumField.size));
    return sum == *stated;
}

std::string pathOfBlock(std::string_view block)
{
    const std::string_view name = untilNul(fieldOf(block, NameField));

    // The GNU format uses the bytes of the prefix for other things.
    const std::string_view prefix = fieldOf(block, MagicField) == PosixMagic ? untilNul(fieldOf(block, PrefixField)) : std::string_view{};
    if (prefix.empty()) {
        return std::string{ name };
    }
    std::string path{ prefix };
    path += '/';
    path += name;
    return path;
}

std::optional<EntryType> entryTypeOf(const char type)
{
    switch (type) {
    case '\0':
    case '0':
    case '7':
        return EntryType::File;
    case '1':
        return EntryType::HardLink;
    case '2':
        return EntryType::SymbolicLink;
    case '5':
        return EntryType::Directory;
    default:
        return std::nullopt;
    }
}

}  // anonymous namespace

void Reader::give(std::string_view input)
{
    _input = input;
}

void Reader::endInput()
{
    _inputEnded = true;
}

bool Reader::needsInput() const
{
    switch (_state) {
    case State::Header:
        return true;
    case State::Metadata:
    case State::Data:
        return _bodyRemaining > 0;
    case State::Padding:
        return _paddingRemaining > 0;
    case State::Ended:
        return false;
    }
    return false;
}

std::string_view Reader::take(const std::uint64_t size)
{
    const std::string_view taken = _input.substr(0, static_cast<std::size_t>(std::min<std::uint64_t>(size, _input.size())));
    _input.remove_prefix(taken.size());
    _offset += taken.size();
    return taken;
}

void Reader::beginBody(const std::uint64_t size, const State state)
{
    _bodyRemaining = size;
    const std::uint64_t pastBlock = size % BlockSize;
    _paddingRemaining = pastBlock == 0 ? 0 : BlockSize - pastBlock;
    _state = state;
}

std::expected<ReadEvent, ReadError> Reader::next()
{
    while (_state != State::Ended) {
        if (needsInput() && _input.empty()) {
            if (_inputEnded) {
                return std::unexpected(TruncatedArchive{});
            }
            return NeedInput{};
        }

        auto event = step();
        if (! event) {
            return std::unexpected(event.error());
        }
        if (*event) {
            return std::move(**event);
        }
    }
    take(_input.size());
    return EndOfArchive{};
}

std::expected<std::optional<ReadEvent>, ReadError> Reader::step()
{
    switch (_state) {
    case State::Header:
        return readHeader();
    case State::Metadata:
        if (auto done = readMetadata(); ! done) {
            return std::unexpected(done.error());
        }
        return std::nullopt;
    case State::Padding:
        skipPadding();
        return std::nullopt;
    case State::Data:
        return readData();
    case State::Ended:
        break;
    }
    return std::nullopt;
}

void Reader::skipPadding()
{
    _paddingRemaining -= take(_paddingRemaining).size();
    if (_paddingRemaining == 0) {
        _state = State::Header;
    }
}

std::optional<ReadEvent> Reader::readData()
{
    if (_bodyRemaining == 0) {
        _state = State::Padding;
        return std::nullopt;
    }
    const std::string_view bytes = take(_bodyRemaining);
    _bodyRemaining -= bytes.size();
    return EntryData{ bytes };
}

InvalidHeader Reader::invalidHere() const
{
    return InvalidHeader{ _headerOffset };
}

std::expected<std::optional<ReadEvent>, ReadError> Reader::readHeader()
{
    if (_blockFill == 0) {
        _headerOffset = _offset;
    }
    const std::string_view piece = take(BlockSize - _blockFill);
    std::copy(piece.begin(), piece.end(), _block.begin() + static_cast<std::ptrdiff_t>(_blockFill));
    _blockFill += piece.size();
    if (_blockFill < BlockSize) {
        return std::nullopt;
    }
    _blockFill = 0;
    return interpretBlock();
}

std::expected<std::optional<ReadEvent>, ReadError> Reader::readZeroBlock()
{
    if (! _sawZeroBlock) {
        _sawZeroBlock = true;
        return std::nullopt;
    }
    // Headers that announced an entry which never came mean the archive is cut.
    if (_overrides.path || _overrides.linkTarget || _overrides.size || _overrides.modificationTime) {
        return std::unexpected(invalidHere());
    }
    _state = State::Ended;
    return EndOfArchive{};
}

std::expected<std::optional<ReadEvent>, ReadError> Reader::interpretBlock()
{
    const std::string_view block{ _block.data(), _block.size() };

    if (std::all_of(block.begin(), block.end(), [](const char byte) {
        return byte == '\0';
    })) {
        return readZeroBlock();
    }
    if (_sawZeroBlock || ! checksumMatches(block)) {
        return std::unexpected(invalidHere());
    }
    const std::string_view magic = fieldOf(block, MagicField);
    if (magic != PosixMagic && magic != GnuMagic) {
        return std::unexpected(invalidHere());
    }

    const auto size = parseNumber(fieldOf(block, SizeField));
    if (! size) {
        return std::unexpected(invalidHere());
    }

    const char type = block[TypeOffset];
    if (const auto kind = metadataKindOf(type)) {
        if (auto begun = beginMetadata(*kind, *size); ! begun) {
            return std::unexpected(begun.error());
        }
        return std::nullopt;
    }

    auto entry = entryFromBlock(type, *size);
    if (! entry) {
        return std::unexpected(entry.error());
    }
    beginBody(entry->size, State::Data);
    return ReadEvent{ std::move(*entry) };
}

std::optional<Reader::MetadataKind> Reader::metadataKindOf(const char type)
{
    switch (type) {
    case 'L':
        return MetadataKind::LongName;
    case 'K':
        return MetadataKind::LongLinkTarget;
    case 'x':
        return MetadataKind::ExtendedHeader;
    case 'g':
        return MetadataKind::GlobalExtendedHeader;
    default:
        return std::nullopt;
    }
}

std::expected<void, ReadError> Reader::beginMetadata(const MetadataKind kind, const std::uint64_t size)
{
    if (size > MaxMetadataSize) {
        return std::unexpected(invalidHere());
    }
    _metadataKind = kind;
    _metadata.clear();
    beginBody(size, State::Metadata);
    return {};
}

std::expected<Entry, ReadError> Reader::entryFromBlock(const char type, const std::uint64_t size)
{
    const std::string_view block{ _block.data(), _block.size() };

    Entry entry;
    entry.path = _overrides.path ? *_overrides.path : pathOfBlock(block);

    const auto entryType = entryTypeOf(type);
    if (! entryType) {
        return std::unexpected(UnsupportedEntry{ entry.path, type });
    }
    entry.type = *entryType;
    entry.linkTarget = _overrides.linkTarget ? *_overrides.linkTarget : std::string{ untilNul(fieldOf(block, LinkField)) };
    entry.size = _overrides.size.value_or(size);

    const auto mode = parseNumber(fieldOf(block, ModeField));
    const auto time = parseNumber(fieldOf(block, TimeField));
    const auto modificationTime = time ? toTime(*time) : std::nullopt;
    if (! mode || ! modificationTime) {
        return std::unexpected(invalidHere());
    }
    entry.mode = static_cast<std::uint32_t>(*mode & 07777U);
    entry.modificationTime = _overrides.modificationTime.value_or(*modificationTime);

    // Only files carry content.
    if (entry.type != EntryType::File && entry.size != 0) {
        return std::unexpected(invalidHere());
    }

    _overrides = {};
    return entry;
}

std::expected<void, ReadError> Reader::readMetadata()
{
    const std::string_view piece = take(_bodyRemaining);
    _metadata.append(piece);
    _bodyRemaining -= piece.size();
    if (_bodyRemaining > 0) {
        return {};
    }
    _state = State::Padding;
    return applyMetadata();
}

std::expected<void, ReadError> Reader::applyMetadata()
{
    if (_metadataKind == MetadataKind::GlobalExtendedHeader) {
        return {};
    }
    if (_metadataKind == MetadataKind::ExtendedHeader) {
        return applyExtendedHeader();
    }

    // A GNU long name ends in NUL, and a NUL inside would cut the path short
    // wherever it is later used.
    std::string name = std::move(_metadata);
    const std::size_t end = name.find_last_not_of('\0');
    name.resize(end == std::string::npos ? 0 : end + 1);
    if (name.find('\0') != std::string::npos) {
        return std::unexpected(invalidHere());
    }
    if (_metadataKind == MetadataKind::LongName) {
        _overrides.path = std::move(name);
    } else {
        _overrides.linkTarget = std::move(name);
    }
    return {};
}

std::expected<void, ReadError> Reader::applyExtendedHeader()
{
    // Each record is "<length of the record> <key>=<value>\n".
    std::string_view records = _metadata;
    while (! records.empty()) {
        const std::size_t space = records.find(' ');
        const auto length = space == std::string_view::npos ? std::nullopt : parseDecimal(records.substr(0, space));
        if (! length || *length <= space + 1 || *length > records.size() || records[*length - 1] != '\n') {
            return std::unexpected(invalidHere());
        }
        const std::string_view record = records.substr(space + 1, *length - space - 2);
        records.remove_prefix(*length);

        const std::size_t equals = record.find('=');
        if (equals == std::string_view::npos) {
            return std::unexpected(invalidHere());
        }
        const std::string_view key = record.substr(0, equals);
        const std::string_view value = record.substr(equals + 1);

        if (key.starts_with("GNU.sparse.")) {
            return std::unexpected(UnsupportedEntry{ _overrides.path.value_or(""), 'S' });
        }
        if (key == "path" || key == "linkpath") {
            if (value.find('\0') != std::string_view::npos) {
                return std::unexpected(invalidHere());
            }
            (key == "path" ? _overrides.path : _overrides.linkTarget) = std::string{ value };
        } else if (key == "size") {
            const auto size = parseDecimal(value);
            if (! size) {
                return std::unexpected(invalidHere());
            }
            _overrides.size = size;
        } else if (key == "mtime") {
            const auto seconds = parseDecimal(value.substr(0, value.find('.')));
            const auto time = seconds ? toTime(*seconds) : std::nullopt;
            if (! time) {
                return std::unexpected(invalidHere());
            }
            _overrides.modificationTime = time;
        }
    }
    return {};
}

}  // namespace scrap::Tar
