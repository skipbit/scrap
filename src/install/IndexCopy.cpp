#include "install/IndexCopy.h"

#include "fetch/FetchErrorText.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "install/UtcTime.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainIndexParser.h"
#include "toolchain/ToolchainVersion.h"

#include <chrono>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

namespace scrap::Install {

namespace {

constexpr std::uint64_t IndexSizeLimit = std::uint64_t{ 16 } * 1024 * 1024;

/**
 * When @p file was last written, in UTC. Saving the index is the only thing that
 * writes the copy, so this is when it was fetched.
 */
std::optional<std::string> modificationTimeInUtc(const std::filesystem::path& file)
{
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(file, ec);
    if (ec) {
        return std::nullopt;
    }
    return utcText(std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(written)));
}

std::expected<Toolchain::ToolchainIndex, Toolchain::ToolchainIndexError> readIndexFile(const std::filesystem::path& file, const Toolchain::ToolchainVersion& scrapVersion)
{
    const auto text = File::readWholeFile(file);
    if (! text.has_value()) {
        return std::unexpected(Toolchain::ToolchainIndexError{ .kind = Toolchain::ToolchainIndexErrorKind::Malformed, .message = "cannot read '" + file.string() + "'" });
    }
    return Toolchain::parseToolchainIndex(*text, scrapVersion);
}

/**
 * Whether the saved copy came from @p url, as the record beside it says.
 */
bool isCopyFrom(const std::filesystem::path& scrapHome, const std::string& url)
{
    const auto recorded = File::readWholeFile(Toolchain::indexCopySourceFile(scrapHome));
    return recorded.has_value() && (*recorded == (url + "\n"));
}

/**
 * The index the saved copy of @p source holds, standing in for one that could
 * not be obtained for @p failure.
 */
std::expected<ObtainedIndex, IndexFault> fallBackOnCopy(const std::filesystem::path& scrapHome, const IndexSource& source, const Toolchain::ToolchainVersion& scrapVersion, IndexUnavailable failure)
{
    auto saved = readSavedIndex(scrapHome, source.url, scrapVersion);
    if (! saved.has_value()) {
        return std::unexpected(std::move(saved.error()));
    }
    std::optional<SavedIndex>& copy = *saved;
    if (! copy.has_value()) {
        return std::unexpected(std::move(failure));
    }
    return ObtainedIndex{ .index = std::move(copy->index), .standIn = CopyStandingIn{ .fetchedAt = std::move(copy->fetchedAt), .reason = std::move(failure.reason) } };
}

}  // anonymous namespace

std::expected<std::optional<SavedIndex>, IndexFormatUnsupported> readSavedIndex(const std::filesystem::path& scrapHome,
                                                                                const std::string& url,
                                                                                const Toolchain::ToolchainVersion& scrapVersion)
{
    const std::filesystem::path copy = Toolchain::indexCopyFile(scrapHome);
    std::error_code ec;
    if ((! std::filesystem::exists(copy, ec)) || (! isCopyFrom(scrapHome, url))) {
        return std::nullopt;
    }
    auto index = readIndexFile(copy, scrapVersion);
    if (! index.has_value()) {
        if (index.error().kind == Toolchain::ToolchainIndexErrorKind::UnsupportedFormat) {
            return std::unexpected(IndexFormatUnsupported{ .format = std::move(index.error().message) });
        }
        return std::nullopt;
    }
    return SavedIndex{ .index = std::move(*index), .fetchedAt = modificationTimeInUtc(copy) };
}

std::expected<FetchedIndex, IndexFault> fetchIndex(const std::filesystem::path& scrapHome,
                                                   const IndexSource& source,
                                                   const Toolchain::ToolchainVersion& scrapVersion)
{
    const std::filesystem::path copy = Toolchain::indexCopyFile(scrapHome);
    std::filesystem::path fetched = copy;
    fetched += ".fetched";

    std::error_code ec;
    std::filesystem::create_directories(copy.parent_path(), ec);
    const auto transfer = Fetch::fetchToFile({ .url = source.url, .destination = fetched, .certificateBundle = source.certificateBundle, .sizeLimit = IndexSizeLimit, .headers = {} });
    if (! transfer.has_value()) {
        return std::unexpected(IndexUnavailable{ .reason = Fetch::describeFetchError(transfer.error()), .fetchError = transfer.error() });
    }

    auto index = readIndexFile(fetched, scrapVersion);
    if (! index.has_value()) {
        std::filesystem::remove(fetched, ec);
        if (index.error().kind == Toolchain::ToolchainIndexErrorKind::UnsupportedFormat) {
            return std::unexpected(IndexFormatUnsupported{ .format = std::move(index.error().message) });
        }
        return std::unexpected(IndexUnavailable{ .reason = std::move(index.error().message), .fetchError = std::nullopt });
    }
    // An index that cannot be saved still answers this install. The record goes
    // first, so a run stopped part way leaves a copy that stands in for nothing.
    const std::filesystem::path sourceFile = Toolchain::indexCopySourceFile(scrapHome);
    std::filesystem::remove(sourceFile, ec);
    std::filesystem::rename(fetched, copy, ec);
    if (! ec) {
        ec = File::replaceFile(sourceFile, source.url + "\n");
    }
    return FetchedIndex{ .index = std::move(*index), .fetchedAt = modificationTimeInUtc(copy) };
}

std::expected<ObtainedIndex, IndexFault> obtainIndex(const std::filesystem::path& scrapHome,
                                                     const IndexSource& source,
                                                     const Toolchain::ToolchainVersion& scrapVersion)
{
    auto fetched = fetchIndex(scrapHome, source, scrapVersion);
    if (fetched.has_value()) {
        return ObtainedIndex{ .index = std::move(fetched->index), .standIn = std::nullopt };
    }
    if (auto* unavailable = std::get_if<IndexUnavailable>(&fetched.error()); unavailable != nullptr) {
        return fallBackOnCopy(scrapHome, source, scrapVersion, std::move(*unavailable));
    }
    return std::unexpected(std::move(fetched.error()));
}

}  // namespace scrap::Install
