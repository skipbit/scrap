#include "install/IngotInstaller.h"

#include "digest/Sha256File.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "install/IngotUnpack.h"
#include "install/UnpackedToolchain.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainIndex.h"

#include <chrono>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <variant>

namespace scrap::Install {

namespace {

constexpr std::uint64_t IngotSizeLimit = std::uint64_t{ 8 } * 1024 * 1024 * 1024;
constexpr std::chrono::hours AbandonedAfter{ 24 };

/**
 * A directory of its own for one run, removed however the run ends unless the
 * run asks to keep part of it.
 */
class WorkDirectory {
public:
    /**
     * A new directory below @p parent. Its name holds this process's id and a
     * count that moves on when another process, one in another container with
     * the same id among them, holds the name.
     */
    [[nodiscard]] static std::expected<WorkDirectory, IngotNotPlaced> below(const std::filesystem::path& parent)
    {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return std::unexpected(IngotNotPlaced{ .path = parent, .code = ec });
        }
        const std::string prefix = "install-" + std::to_string(::getpid()) + "-";
        for (unsigned count = 0;; ++count) {
            std::filesystem::path candidate = parent / (prefix + std::to_string(count));
            if (std::filesystem::create_directory(candidate, ec)) {
                return WorkDirectory{ std::move(candidate) };
            }
            if (ec) {
                return std::unexpected(IngotNotPlaced{ .path = candidate, .code = ec });
            }
        }
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

    /**
     * Leave @p kept behind when the run ends, and nothing else of the run.
     */
    void keepOnly(const std::filesystem::path& kept)
    {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(_path, ec), end; (! ec) && (it != end); it.increment(ec)) {
            if (it->path() != kept) {
                std::error_code entryError;
                std::filesystem::remove_all(it->path(), entryError);
            }
        }
        _path.clear();
    }

private:
    explicit WorkDirectory(std::filesystem::path path)
        : _path(std::move(path))
    {
    }

    std::filesystem::path _path;
};

/**
 * Put @p finished at @p destination, moving aside what is there to
 * @p setAside, and bringing it back when the move fails. What cannot be brought
 * back is named in the failure.
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
            if (ec) {
                return std::unexpected(IngotNotPlaced{ .path = destination, .code = failure, .previousKeptAt = setAside });
            }
        }
        return std::unexpected(IngotNotPlaced{ .path = destination, .code = failure });
    }
    return {};
}

/**
 * Fetch the ingot @p source names to @p ingot, and check it against the index.
 */
std::expected<void, IngotInstallFailure> fetchChecked(const Toolchain::UrlSource& source, const std::filesystem::path& ingot, const std::optional<std::filesystem::path>& certificateBundle)
{
    const auto fetched = Fetch::fetchToFile({ .url = source.url, .destination = ingot, .certificateBundle = certificateBundle, .sizeLimit = IngotSizeLimit });
    if (! fetched.has_value()) {
        return std::unexpected(IngotNotFetched{ .error = fetched.error() });
    }
    const auto digest = Digest::sha256OfFile(ingot);
    if (! digest.has_value()) {
        return std::unexpected(IngotNotUnpacked{ .reason = "cannot read the ingot: " + digest.error().message(), .code = digest.error() });
    }
    if (*digest != source.sha256) {
        return std::unexpected(IngotDoesNotMatch{});
    }
    return {};
}

/**
 * Unpack @p ingot in the new directory @p stage and record what it holds.
 *
 * @return Where the finished toolchain starts, or what went wrong.
 */
std::expected<std::filesystem::path, IngotInstallFailure> prepareToolchain(const Toolchain::SelectedIngot& selected, const std::filesystem::path& ingot, const std::filesystem::path& stage)
{
    std::error_code ec;
    std::filesystem::create_directory(stage, ec);
    if (ec) {
        return std::unexpected(IngotNotPlaced{ .path = stage, .code = ec });
    }
    if (auto unpacked = unpack(ingot, stage); ! unpacked.has_value()) {
        return std::unexpected(std::move(unpacked.error()));
    }

    std::filesystem::path root = contentRoot(stage);
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
    return root;
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
    if (auto fetched = fetchChecked(*source, ingot, request.certificateBundle); ! fetched.has_value()) {
        return std::unexpected(std::move(fetched.error()));
    }

    request.announce("Extracting '" + name + "'");
    auto root = prepareToolchain(selected, ingot, work->path() / "stage");
    if (! root.has_value()) {
        return std::unexpected(std::move(root.error()));
    }

    std::filesystem::path destination = Toolchain::toolchainDirectory(request.scrapHome, selected.family, selected.version.text(), request.environment.directoryName());
    if (auto placed = place(*root, destination, work->path() / "replaced"); ! placed.has_value()) {
        if (placed.error().previousKeptAt.has_value()) {
            work->keepOnly(*placed.error().previousKeptAt);
        }
        return std::unexpected(std::move(placed.error()));
    }
    return destination;
}

}  // namespace scrap::Install
