#include "install/IngotInstaller.h"

#include "digest/Sha256File.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "install/IngotUnpack.h"
#include "install/UnpackedToolchain.h"
#include "install/UtcTime.h"
#include "install/WorkDirectory.h"
#include "registry/IngotLocation.h"
#include "registry/RegistryFetch.h"
#include "toolchain/IngotSelection.h"
#include "toolchain/InstallRecord.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainIndex.h"

#include <chrono>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace scrap::Install {

namespace {

constexpr std::uint64_t IngotSizeLimit = std::uint64_t{ 8 } * 1024 * 1024 * 1024;

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
 * Where a fetched ingot goes, and what fetching it may use.
 */
struct IngotTransfer {
    std::filesystem::path ingot;
    std::optional<std::filesystem::path> certificateBundle;
};

/**
 * Fetch the ingot @p source names, and check it against the index.
 */
std::expected<void, IngotInstallFailure> fetchChecked(const Toolchain::UrlSource& source, const IngotTransfer& transfer)
{
    const std::filesystem::path& ingot = transfer.ingot;
    const auto fetched = Fetch::fetchToFile({ .url = source.url, .destination = ingot, .certificateBundle = transfer.certificateBundle, .sizeLimit = IngotSizeLimit, .headers = {} });
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

IngotInstallFailure installFailureOf(Fetch::FetchError error)
{
    return IngotNotFetched{ .error = std::move(error) };
}

IngotInstallFailure installFailureOf(const Registry::ContentDoesNotMatch&)
{
    return IngotDoesNotMatch{};
}

IngotInstallFailure installFailureOf(Registry::AnswerUnusable answer)
{
    return RegistryAnswerUnusable{ .url = std::move(answer.url), .reason = std::move(answer.reason) };
}

IngotInstallFailure installFailureOf(const Registry::FetchedFileUnreadable& unreadable)
{
    return IngotNotUnpacked{ .reason = "cannot read '" + unreadable.path.string() + "': " + unreadable.code.message(), .code = unreadable.code };
}

/**
 * Fetch the ingot @p source names from its registry, which checks it against
 * the digest of the index.
 */
std::expected<void, IngotInstallFailure> fetchChecked(const Toolchain::RegistrySource& source, const IngotTransfer& transfer)
{
    auto location = Registry::locateIngot(source.registry, source.tag, source.digest);
    if (! location.has_value()) {
        return std::unexpected(RegistryNotLocated{ .registry = source.registry, .tag = source.tag });
    }
    auto fetched = Registry::fetchIngot({ .location = std::move(*location),
                                          .destination = transfer.ingot,
                                          .certificateBundle = transfer.certificateBundle,
                                          .sizeLimit = IngotSizeLimit });
    if (! fetched.has_value()) {
        return std::unexpected(std::visit(
            [](auto&& failure) {
            return installFailureOf(std::forward<decltype(failure)>(failure));
        },
            std::move(fetched.error())));
    }
    return {};
}

/**
 * The program @p name below bin/ of @p root, or why the ingot does not hold it.
 */
std::expected<std::filesystem::path, IngotInstallFailure> programIn(const std::filesystem::path& root, std::string_view name)
{
    std::filesystem::path program = std::filesystem::path{ "bin" } / name;
    std::error_code ec;
    if (! std::filesystem::exists(root / program, ec)) {
        return std::unexpected(IngotNotUnpacked{ .reason = "the ingot holds no '" + program.string() + "'", .code = {} });
    }
    return program;
}

/**
 * Unpack @p ingot in the new directory @p stage and record what it holds, and
 * the time @p now tells once it is unpacked.
 *
 * @return Where the finished toolchain starts, or what went wrong.
 */
std::expected<std::filesystem::path, IngotInstallFailure> prepareToolchain(const Toolchain::SelectedIngot& selected,
                                                                           const std::function<std::chrono::system_clock::time_point()>& now,
                                                                           const std::filesystem::path& ingot,
                                                                           const std::filesystem::path& stage)
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
    auto compiler = programIn(root, selected.compiler);
    if (! compiler.has_value()) {
        return std::unexpected(std::move(compiler.error()));
    }
    std::optional<std::filesystem::path> cCompiler;
    if (selected.cCompiler.has_value()) {
        auto found = programIn(root, *selected.cCompiler);
        if (! found.has_value()) {
            return std::unexpected(std::move(found.error()));
        }
        cCompiler = std::move(*found);
    }
    const Toolchain::InstallRecord record{ .compiler = std::move(*compiler),
                                           .runtimeLibraryDirectories = findRuntimeLibraryDirectories(root),
                                           .arguments = runtimeArgumentsFor(selected.ingot.platform, root),
                                           .ingotIntegrity = Toolchain::integrityOf(selected.ingot),
                                           .cCompiler = std::move(cCompiler),
                                           .source = Toolchain::sourceOf(selected.ingot),
                                           .installedAt = utcText(now()) };
    const std::filesystem::path recordFile = root / Toolchain::InstallRecordName;
    ec = File::writeNewFile(recordFile, Toolchain::renderInstallRecord(record));
    if (ec) {
        return std::unexpected(IngotNotPlaced{ .path = recordFile, .code = ec });
    }
    return root;
}

}  // anonymous namespace

std::expected<std::filesystem::path, IngotInstallFailure> installIngot(const IngotInstallRequest& request)
{
    const Toolchain::SelectedIngot& selected = request.selected;
    const std::filesystem::path workRoot = Toolchain::temporaryDirectory(request.scrapHome);
    discardAbandonedWork(workRoot, std::filesystem::file_time_type::clock::now());
    auto work = WorkDirectory::below(workRoot, "install");
    if (! work.has_value()) {
        return std::unexpected(IngotNotPlaced{ .path = std::move(work.error().path), .code = work.error().code });
    }

    const std::string name = selected.family + "@" + selected.version.text();
    const std::filesystem::path ingot = work->path() / "ingot";
    request.announce("Downloading '" + name + "'");
    const IngotTransfer transfer{ .ingot = ingot, .certificateBundle = request.certificateBundle };
    auto fetched = std::visit(
        [&transfer](const auto& source) {
        return fetchChecked(source, transfer);
    },
        selected.ingot.source);
    if (! fetched.has_value()) {
        return std::unexpected(std::move(fetched.error()));
    }

    request.announce("Extracting '" + name + "'");
    auto root = prepareToolchain(selected, request.now, ingot, work->path() / "stage");
    if (! root.has_value()) {
        return std::unexpected(std::move(root.error()));
    }

    std::filesystem::path destination = Toolchain::toolchainDirectory(request.scrapHome, selected.family, selected.version.text(), request.environment.directoryName());
    if (auto placed = place(*root, destination, work->path() / "replaced"); ! placed.has_value()) {
        const std::optional<std::filesystem::path>& kept = placed.error().previousKeptAt;
        if (kept.has_value()) {
            work->keepOnly(*kept);
        }
        return std::unexpected(std::move(placed.error()));
    }
    return destination;
}

}  // namespace scrap::Install
