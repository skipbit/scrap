#include "tar/TarExtractor.h"

#include "tar/Descriptor.h"
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

/// Whether a walk failed by going through something that is not a directory.
bool wentThroughNonDirectory(const std::error_code code)
{
    return code == std::errc::too_many_symbolic_link_levels || code == std::errc::not_a_directory;
}

/// The failure of walking @p path: going through anything but a directory is the
/// archive's doing, the rest is the operating system's.
ExtractError walkFailure(const std::string& path, const std::error_code code)
{
    if (wentThroughNonDirectory(code)) {
        return UnsafeMemberPath{ path };
    }
    return CannotWriteMember{ path, code };
}

}  // anonymous namespace

Extractor::Extractor(Descriptor root)
    : _root{ std::move(root) }
{
}

std::expected<Extractor, ExtractError> Extractor::into(const std::filesystem::path& directory)
{
    const int descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (descriptor < 0) {
        return std::unexpected(CannotOpenDirectory{ directory, lastFailure() });
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
std::expected<Descriptor, std::error_code> walkDirectories(const Descriptor& root, std::span<const std::string_view> components, const Creation creation)
{
    Descriptor current{ ::openat(root.get(), ".", DirectoryFlags) };
    if (current.get() < 0) {
        return std::unexpected(lastFailure());
    }
    for (std::size_t index = 0; index < components.size(); ++index) {
        const std::string name = nameOf(components[index]);
        Descriptor next{ ::openat(current.get(), name.c_str(), DirectoryFlags) };
        if (next.get() < 0 && errno == ENOENT && creation.allowed) {
            const mode_t mode = index + 1 == components.size() ? creation.lastMode : ImpliedDirectoryMode;
            if (::mkdirat(current.get(), name.c_str(), mode) == 0) {
                next = Descriptor{ ::openat(current.get(), name.c_str(), DirectoryFlags) };
            }
        }
        if (next.get() < 0) {
            return std::unexpected(lastFailure());
        }
        current = std::move(next);
    }
    return current;
}

/// Opens the directory that holds the member at @p components, creating what is
/// missing on the way.
std::expected<Descriptor, ExtractError> openParent(const Descriptor& root, const std::string& path, std::span<const std::string_view> components)
{
    auto parent = walkDirectories(root, components.first(components.size() - 1), { .allowed = true });
    if (! parent) {
        return std::unexpected(walkFailure(path, parent.error()));
    }
    return std::move(*parent);
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
    if (entry.type != EntryType::Directory && components->empty()) {
        return std::unexpected(UnsafeMemberPath{ entry.path });
    }

    switch (entry.type) {
    case EntryType::Directory:
        return makeDirectory(entry, *components);
    case EntryType::SymbolicLink:
        return makeSymbolicLink(entry, *components);
    case EntryType::HardLink:
        return makeHardLink(entry, *components);
    case EntryType::File:
        return beginFile(entry, *components);
    }
    return {};
}

std::expected<void, ExtractError> Extractor::makeDirectory(const Entry& entry, std::span<const std::string_view> components) const
{
    const mode_t mode = (static_cast<mode_t>(entry.mode) & PermissionBits) | OwnerCanUse;
    const auto directory = walkDirectories(_root, components, { .allowed = true, .lastMode = mode });
    if (! directory) {
        return std::unexpected(walkFailure(entry.path, directory.error()));
    }
    return {};
}

std::expected<void, ExtractError> Extractor::makeSymbolicLink(const Entry& entry, std::span<const std::string_view> components) const
{
    if (! symbolicLinkStaysInside(components, entry.linkTarget)) {
        return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
    }
    const auto parent = openParent(_root, entry.path, components);
    if (! parent) {
        return std::unexpected(parent.error());
    }
    if (::symlinkat(entry.linkTarget.c_str(), parent->get(), nameOf(components.back()).c_str()) != 0) {
        return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
    }
    return {};
}

std::expected<void, ExtractError> Extractor::beginFile(const Entry& entry, std::span<const std::string_view> components)
{
    const auto parent = openParent(_root, entry.path, components);
    if (! parent) {
        return std::unexpected(parent.error());
    }
    const int descriptor = ::openat(parent->get(), nameOf(components.back()).c_str(), NewFileFlags, static_cast<mode_t>(entry.mode) & PermissionBits);
    if (descriptor < 0) {
        return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
    }
    _file = Descriptor{ descriptor };
    _filePath = entry.path;
    _fileModificationTime = entry.modificationTime;
    return {};
}

std::expected<void, ExtractError> Extractor::makeHardLink(const Entry& entry, std::span<const std::string_view> components) const
{
    const auto targetComponents = memberComponents(entry.linkTarget);
    if (! targetComponents || targetComponents->empty()) {
        return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
    }
    const std::span<const std::string_view> target{ *targetComponents };

    const auto parent = openParent(_root, entry.path, components);
    if (! parent) {
        return std::unexpected(parent.error());
    }

    const auto targetParent = walkDirectories(_root, target.first(target.size() - 1), { .allowed = false });
    if (! targetParent) {
        if (wentThroughNonDirectory(targetParent.error())) {
            return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
        }
        return std::unexpected(CannotWriteMember{ entry.path, targetParent.error() });
    }
    const std::string targetName = nameOf(target.back());

    struct stat stated { };
    if (::fstatat(targetParent->get(), targetName.c_str(), &stated, AT_SYMLINK_NOFOLLOW) != 0) {
        return std::unexpected(CannotWriteMember{ entry.path, lastFailure() });
    }
    if (! S_ISREG(stated.st_mode)) {
        return std::unexpected(UnsafeLinkTarget{ entry.path, entry.linkTarget });
    }

    if (::linkat(targetParent->get(), targetName.c_str(), parent->get(), nameOf(components.back()).c_str(), 0) != 0) {
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
        if (written == 0) {
            // POSIX allows a write of zero bytes to report nothing written,
            // which would leave the loop where it started.
            return std::unexpected(CannotWriteMember{ _filePath, std::make_error_code(std::errc::io_error) });
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
