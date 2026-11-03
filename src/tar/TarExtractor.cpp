#include "tar/TarExtractor.h"

#include "tar/TarPath.h"
#include "tar/TarReader.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <ctime>
#include <expected>  // IWYU pragma: keep
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <variant>

namespace scrap::Tar {

namespace {

constexpr mode_t PermissionBits = 0777;
constexpr mode_t OwnerCanUse = 0700;
constexpr mode_t ImpliedDirectoryMode = 0777;

constexpr int DirectoryFlags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
constexpr int NewFileFlags = O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;

std::error_code lastFailure()
{
    return { errno, std::generic_category() };
}

/// The name of @p component as the C string the system calls take.
std::string nameOf(std::string_view component)
{
    return std::string{ component };
}

/// The failure of walking @p path: going through anything but a directory is the
/// archive's doing, the rest is the operating system's.
ExtractError walkFailure(const std::string& path, const std::error_code code)
{
    if (code == std::errc::too_many_symbolic_link_levels || code == std::errc::not_a_directory) {
        return UnsafeMemberPath{ path };
    }
    return CannotWriteMember{ path, code };
}

}  // anonymous namespace

Extractor::Descriptor::Descriptor(const int value)
    : _value{ value }
{
}

Extractor::Descriptor::Descriptor(Descriptor&& other) noexcept
    : _value{ std::exchange(other._value, -1) }
{
}

Extractor::Descriptor& Extractor::Descriptor::operator=(Descriptor&& other) noexcept
{
    if (this != &other) {
        discard();
        _value = std::exchange(other._value, -1);
    }
    return *this;
}

Extractor::Descriptor::~Descriptor()
{
    discard();
}

void Extractor::Descriptor::discard() noexcept
{
    if (_value >= 0) {
        ::close(std::exchange(_value, -1));
    }
}

std::error_code Extractor::Descriptor::close()
{
    if (_value < 0) {
        return {};
    }
    const int result = ::close(std::exchange(_value, -1));
    return result < 0 ? lastFailure() : std::error_code{};
}

Extractor::Extractor(Descriptor root)
    : _root{ std::move(root) }
{
}

std::expected<Extractor, ExtractError> Extractor::into(const std::filesystem::path& directory)
{
    const int descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0) {
        return std::unexpected(CannotWriteMember{ directory.string(), lastFailure() });
    }
    return Extractor{ Descriptor{ descriptor } };
}

std::expected<void, ExtractError> Extractor::feed(std::string_view piece)
{
    _reader.give(piece);
    return drain();
}

std::expected<void, ExtractError> Extractor::finish()
{
    _reader.endInput();
    return drain();
}

std::expected<void, ExtractError> Extractor::drain()
{
    while (true) {
        const auto event = _reader.next();
        if (! event) {
            return std::unexpected(event.error());
        }
        if (std::holds_alternative<NeedInput>(*event)) {
            return {};
        }
        if (std::holds_alternative<EndOfArchive>(*event)) {
            return endFile();
        }

        const auto* entry = std::get_if<Entry>(&*event);
        auto step = entry != nullptr ? begin(*entry) : writeContent(std::get<EntryData>(*event).bytes);
        if (! step) {
            return step;
        }
    }
}

namespace {

/// Whether the walk may create what is missing, and with which permissions the
/// last directory of the walk is created.
struct Creation {
    bool allowed = false;
    mode_t lastMode = ImpliedDirectoryMode;
};

/// Opens the directory at @p components below @p root, following no symbolic
/// link on the way.
std::expected<int, std::error_code> walkDirectories(const int root, std::span<const std::string_view> components, const Creation creation)
{
    int current = ::openat(root, ".", DirectoryFlags);
    if (current < 0) {
        return std::unexpected(lastFailure());
    }
    for (std::size_t index = 0; index < components.size(); ++index) {
        const std::string name = nameOf(components[index]);
        int next = ::openat(current, name.c_str(), DirectoryFlags);
        if (next < 0 && errno == ENOENT && creation.allowed) {
            const mode_t mode = index + 1 == components.size() ? creation.lastMode : ImpliedDirectoryMode;
            if (::mkdirat(current, name.c_str(), mode) == 0) {
                next = ::openat(current, name.c_str(), DirectoryFlags);
            }
        }
        const std::error_code failure = next < 0 ? lastFailure() : std::error_code{};
        ::close(current);
        if (next < 0) {
            return std::unexpected(failure);
        }
        current = next;
    }
    return current;
}

}  // anonymous namespace

std::expected<void, ExtractError> Extractor::begin(const Entry& entry)
{
    if (auto ended = endFile(); ! ended) {
        return ended;
    }

    const auto components = memberComponents(entry.path);
    if (! components) {
        return std::unexpected(UnsafeMemberPath{ entry.path });
    }

    if (entry.type == EntryType::Directory) {
        const mode_t mode = (static_cast<mode_t>(entry.mode) & PermissionBits) | OwnerCanUse;
        const auto directory = walkDirectories(_root.get(), *components, { .allowed = true, .lastMode = mode });
        if (! directory) {
            return std::unexpected(walkFailure(entry.path, directory.error()));
        }
        const Descriptor opened{ *directory };
        return {};
    }

    if (components->empty()) {
        return std::unexpected(UnsafeMemberPath{ entry.path });
    }
    const std::span<const std::string_view> all{ *components };
    const auto parentDirectory = walkDirectories(_root.get(), all.first(all.size() - 1), { .allowed = true });
    if (! parentDirectory) {
        return std::unexpected(walkFailure(entry.path, parentDirectory.error()));
    }
    const Descriptor parent{ *parentDirectory };
    const std::string_view name = all.back();

    switch (entry.type) {
    case EntryType::SymbolicLink: {
        if (! symbolicLinkStaysInside(all, entry.linkTarget)) {
            return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
        }
        if (::symlinkat(entry.linkTarget.c_str(), parent.get(), nameOf(name).c_str()) != 0) {
            return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
        }
        return {};
    }
    case EntryType::HardLink:
        return makeHardLink(entry, parent, name);
    case EntryType::File:
    case EntryType::Directory:
        break;
    }
    return beginFile(entry, parent, name);
}

std::expected<void, ExtractError> Extractor::beginFile(const Entry& entry, const Descriptor& parent, std::string_view name)
{
    const int descriptor = ::openat(parent.get(), nameOf(name).c_str(), NewFileFlags, static_cast<mode_t>(entry.mode) & PermissionBits);
    if (descriptor < 0) {
        return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
    }
    _file = Descriptor{ descriptor };
    _filePath = entry.path;
    _fileModificationTime = entry.modificationTime;
    return {};
}

std::expected<void, ExtractError> Extractor::makeHardLink(const Entry& entry, const Descriptor& parent, std::string_view name) const
{
    const auto targetComponents = memberComponents(entry.linkTarget);
    if (! targetComponents || targetComponents->empty()) {
        return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
    }
    const std::span<const std::string_view> all{ *targetComponents };

    const auto targetDirectory = walkDirectories(_root.get(), all.first(all.size() - 1), { .allowed = false });
    if (! targetDirectory) {
        const std::error_code code = targetDirectory.error();
        if (code == std::errc::too_many_symbolic_link_levels || code == std::errc::not_a_directory) {
            return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
        }
        return std::unexpected(CannotWriteMember{ entry.path, code });
    }
    const Descriptor targetParent{ *targetDirectory };
    const std::string targetName = nameOf(all.back());

    struct stat target { };
    if (::fstatat(targetParent.get(), targetName.c_str(), &target, AT_SYMLINK_NOFOLLOW) != 0) {
        return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
    }
    if (! S_ISREG(target.st_mode)) {
        return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
    }

    if (::linkat(targetParent.get(), targetName.c_str(), parent.get(), nameOf(name).c_str(), 0) != 0) {
        return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
    }
    return {};
}

std::expected<void, ExtractError> Extractor::writeContent(std::string_view bytes)
{
    while (! bytes.empty()) {
        const ssize_t written = ::write(_file.get(), bytes.data(), bytes.size());
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected(CannotWriteMember{ _filePath, lastFailure() });
        }
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
    return {};
}

std::expected<void, ExtractError> Extractor::endFile()
{
    if (_file.get() < 0) {
        return {};
    }
    const std::array<timespec, 2> times{ timespec{ .tv_sec = static_cast<time_t>(_fileModificationTime), .tv_nsec = 0 }, timespec{ .tv_sec = static_cast<time_t>(_fileModificationTime), .tv_nsec = 0 } };
    if (::futimens(_file.get(), times.data()) != 0) {
        return std::unexpected(CannotWriteMember{ _filePath, lastFailure() });
    }
    if (const std::error_code failure = _file.close()) {
        return std::unexpected(CannotWriteMember{ _filePath, failure });
    }
    return {};
}

}  // namespace scrap::Tar
