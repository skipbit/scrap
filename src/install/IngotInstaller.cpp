#include "install/IngotInstaller.h"

#include "digest/Sha256File.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "tar/TarExtractor.h"
#include "tar/TarReader.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainIndex.h"
#include "xz/XzFile.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Install {

namespace {

constexpr std::uint64_t IngotSizeLimit = std::uint64_t{ 8 } * 1024 * 1024 * 1024;
constexpr std::chrono::hours AbandonedAfter{ 24 };

/**
 * A directory of its own for one run, removed however the run ends.
 */
class WorkDirectory {
public:
    [[nodiscard]] static std::expected<WorkDirectory, IngotNotPlaced> below(const std::filesystem::path& parent)
    {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return std::unexpected(IngotNotPlaced{ .path = parent, .code = ec });
        }
        std::string pattern = (parent / "install-XXXXXX").string();
        if (::mkdtemp(pattern.data()) == nullptr) {
            return std::unexpected(IngotNotPlaced{ .path = parent, .code = std::error_code{ errno, std::generic_category() } });
        }
        return WorkDirectory{ std::filesystem::path{ pattern } };
    }

    WorkDirectory(const WorkDirectory&) = delete;
    WorkDirectory& operator=(const WorkDirectory&) = delete;
    WorkDirectory(WorkDirectory&& other) noexcept
        : _path(std::exchange(other._path, {}))
    {
    }
    WorkDirectory& operator=(WorkDirectory&&) = delete;
    ~WorkDirectory()
    {
        if (! _path.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(_path, ec);
        }
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
        return _path;
    }

private:
    explicit WorkDirectory(std::filesystem::path path)
        : _path(std::move(path))
    {
    }

    std::filesystem::path _path;
};

std::string describe(const Tar::ReadError& error)
{
    return std::visit([]<typename Alternative>(const Alternative& alternative) -> std::string {
        if constexpr (std::is_same_v<Alternative, Tar::TruncatedArchive>) {
            return "the archive ends too soon";
        } else if constexpr (std::is_same_v<Alternative, Tar::InvalidHeader>) {
            return "the archive has an invalid header at offset " + std::to_string(alternative.offset);
        } else {
            return "the archive holds '" + alternative.path + "', a kind of entry that is not supported";
        }
    }, error);
}

IngotNotUnpacked unpackFailure(const Tar::ExtractError& error)
{
    return std::visit([]<typename Alternative>(const Alternative& alternative) -> IngotNotUnpacked {
        if constexpr (std::is_same_v<Alternative, Tar::ReadError>) {
            return { .reason = describe(alternative), .code = {} };
        } else if constexpr (std::is_same_v<Alternative, Tar::CannotOpenDirectory>) {
            return { .reason = "cannot open '" + alternative.path.string() + "': " + alternative.code.message(), .code = alternative.code };
        } else if constexpr (std::is_same_v<Alternative, Tar::UnsafeMemberPath>) {
            return { .reason = "the archive holds the unsafe path '" + alternative.path + "'", .code = {} };
        } else if constexpr (std::is_same_v<Alternative, Tar::UnsafeLinkTarget>) {
            return { .reason = "the archive links '" + alternative.path + "' to '" + alternative.target + "', which is not allowed", .code = {} };
        } else {
            return { .reason = "cannot write '" + alternative.path + "': " + alternative.code.message(), .code = alternative.code };
        }
    }, error);
}

IngotNotUnpacked unpackFailure(const Xz::DecodeError& error)
{
    return std::visit([]<typename Alternative>(const Alternative& alternative) -> IngotNotUnpacked {
        if constexpr (std::is_same_v<Alternative, Xz::CannotReadFile>) {
            return { .reason = "cannot read the ingot: " + alternative.code.message(), .code = alternative.code };
        } else if constexpr (std::is_same_v<Alternative, Xz::NotAnXzFile>) {
            return { .reason = "the ingot is not an xz file", .code = {} };
        } else if constexpr (std::is_same_v<Alternative, Xz::CorruptData>) {
            return { .reason = "the ingot is corrupt", .code = {} };
        } else if constexpr (std::is_same_v<Alternative, Xz::TruncatedData>) {
            return { .reason = "the ingot ends too soon", .code = {} };
        } else if constexpr (std::is_same_v<Alternative, Xz::OutOfMemory>) {
            return { .reason = "not enough memory to decode the ingot", .code = std::make_error_code(std::errc::not_enough_memory) };
        } else {
            return { .reason = "decoding the ingot stopped", .code = {} };
        }
    }, error);
}

std::expected<void, IngotNotUnpacked> unpack(const std::filesystem::path& ingot, const std::filesystem::path& directory)
{
    auto extractor = Tar::Extractor::into(directory);
    if (! extractor.has_value()) {
        return std::unexpected(unpackFailure(extractor.error()));
    }
    std::optional<Tar::ExtractError> extractFailure;
    const auto decoded = Xz::decodeFile(ingot, [&extractor, &extractFailure](const std::string_view piece) {
        auto fed = extractor->feed(piece);
        if (! fed.has_value()) {
            extractFailure = std::move(fed.error());
            return false;
        }
        return true;
    });
    if (extractFailure.has_value()) {
        return std::unexpected(unpackFailure(*extractFailure));
    }
    if (! decoded.has_value()) {
        return std::unexpected(unpackFailure(decoded.error()));
    }
    if (auto finished = extractor->finish(); ! finished.has_value()) {
        return std::unexpected(unpackFailure(finished.error()));
    }
    return {};
}

/**
 * Where the toolchain starts in what was unpacked into @p directory: an archive
 * may hold it all in one directory of its own.
 */
std::filesystem::path contentRoot(const std::filesystem::path& directory)
{
    std::error_code ec;
    std::filesystem::directory_iterator entries(directory, ec);
    if (ec || (entries == std::filesystem::directory_iterator{})) {
        return directory;
    }
    const std::filesystem::path only = entries->path();
    entries.increment(ec);
    if (ec || (entries != std::filesystem::directory_iterator{}) || (only.filename() == "bin")
        || (! std::filesystem::is_directory(std::filesystem::symlink_status(only, ec)))) {
        return directory;
    }
    return only;
}

bool isRuntimeLibraryName(const std::string& name)
{
    return name.starts_with("libstdc++.so") || name.starts_with("libc++.so") || (name.starts_with("libc++.") && name.ends_with(".dylib"));
}

/**
 * The directories below @p root, relative to it, that hold a C++ runtime library.
 */
std::vector<std::filesystem::path> findRuntimeLibraryDirectories(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> found;
    for (const std::string_view top : { "lib", "lib64" }) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(root / top, ec), end; (! ec) && (it != end); it.increment(ec)) {
            if (isRuntimeLibraryName(it->path().filename().string())) {
                found.push_back(it->path().parent_path().lexically_relative(root));
            }
        }
    }
    std::ranges::sort(found);
    const auto repeated = std::ranges::unique(found);
    found.erase(repeated.begin(), repeated.end());
    return found;
}

/**
 * Put @p finished at @p destination, moving aside what is there to
 * @p setAside, and bringing it back when the move fails.
 */
std::expected<void, IngotNotPlaced> place(const std::filesystem::path& finished, const std::filesystem::path& destination, const std::filesystem::path& setAside)
{
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        return std::unexpected(IngotNotPlaced{ .path = destination.parent_path(), .code = ec });
    }
    const bool occupied = std::filesystem::exists(std::filesystem::symlink_status(destination, ec));
    if (occupied) {
        std::filesystem::rename(destination, setAside, ec);
        if (ec) {
            return std::unexpected(IngotNotPlaced{ .path = destination, .code = ec });
        }
    }
    std::filesystem::rename(finished, destination, ec);
    if (ec) {
        const std::error_code failure = ec;
        if (occupied) {
            std::filesystem::rename(setAside, destination, ec);
        }
        return std::unexpected(IngotNotPlaced{ .path = destination, .code = failure });
    }
    return {};
}

}  // anonymous namespace

void discardAbandonedWork(const std::filesystem::path& workDirectory, const std::filesystem::file_time_type now)
{
    std::error_code ec;
    for (std::filesystem::directory_iterator it(workDirectory, ec), end; (! ec) && (it != end); it.increment(ec)) {
        std::error_code entryError;
        const auto written = std::filesystem::last_write_time(it->path(), entryError);
        if ((! entryError) && ((now - written) >= AbandonedAfter)) {
            std::filesystem::remove_all(it->path(), entryError);
        }
    }
}

std::expected<std::filesystem::path, IngotInstallFailure> installIngot(const IngotInstallRequest& request)
{
    const Toolchain::SelectedIngot& selected = request.selected;
    const auto* source = std::get_if<Toolchain::UrlSource>(&selected.ingot.source);
    if (source == nullptr) {
        return std::unexpected(RegistryNotSupported{});
    }

    const std::filesystem::path workRoot = Toolchain::temporaryDirectory(request.scrapHome);
    discardAbandonedWork(workRoot, std::filesystem::file_time_type::clock::now());
    auto work = WorkDirectory::below(workRoot);
    if (! work.has_value()) {
        return std::unexpected(std::move(work.error()));
    }

    const std::string name = selected.family + "@" + selected.version.text();
    const std::filesystem::path ingot = work->path() / "ingot";
    request.announce("Downloading '" + name + "'");
    const auto fetched = Fetch::fetchToFile({ .url = source->url, .destination = ingot, .certificateBundle = request.certificateBundle, .sizeLimit = IngotSizeLimit });
    if (! fetched.has_value()) {
        return std::unexpected(IngotNotFetched{ .error = fetched.error() });
    }

    const auto digest = Digest::sha256OfFile(ingot);
    if (! digest.has_value()) {
        return std::unexpected(IngotNotUnpacked{ .reason = "cannot read the ingot: " + digest.error().message(), .code = digest.error() });
    }
    if (*digest != source->sha256) {
        return std::unexpected(IngotDoesNotMatch{});
    }

    request.announce("Extracting '" + name + "'");
    const std::filesystem::path stage = work->path() / "stage";
    std::error_code ec;
    std::filesystem::create_directory(stage, ec);
    if (ec) {
        return std::unexpected(IngotNotPlaced{ .path = stage, .code = ec });
    }
    if (auto unpacked = unpack(ingot, stage); ! unpacked.has_value()) {
        return std::unexpected(std::move(unpacked.error()));
    }

    const std::filesystem::path root = contentRoot(stage);
    const std::filesystem::path compiler = std::filesystem::path{ "bin" } / selected.compiler;
    if (! std::filesystem::exists(root / compiler, ec)) {
        return std::unexpected(IngotNotUnpacked{ .reason = "the ingot holds no '" + compiler.string() + "'", .code = {} });
    }
    const Toolchain::InstallRecord record{ .compiler = compiler,
                                           .runtimeLibraryDirectories = findRuntimeLibraryDirectories(root),
                                           .ingotIntegrity = Toolchain::integrityOf(selected.ingot) };
    const std::filesystem::path recordFile = root / Toolchain::InstallRecordName;
    ec = File::writeNewFile(recordFile, Toolchain::renderInstallRecord(record));
    if (ec) {
        return std::unexpected(IngotNotPlaced{ .path = recordFile, .code = ec });
    }

    const std::filesystem::path destination = Toolchain::toolchainDirectory(request.scrapHome, selected.family, selected.version.text(), request.environment.directoryName());
    if (auto placed = place(root, destination, work->path() / "replaced"); ! placed.has_value()) {
        return std::unexpected(std::move(placed.error()));
    }
    return destination;
}

}  // namespace scrap::Install
