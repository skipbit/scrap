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
#include <cstddef>
#include <cstdint>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace scrap::Install {

namespace {

constexpr std::uint64_t IndexSizeLimit = std::uint64_t{ 16 } * 1024 * 1024;

/**
 * When @p file was last written, in UTC. Saving the index is the only thing that
 * writes the copy, so this is when it was fetched by a scrap that did not record
 * the time.
 */
std::string modificationTimeInUtc(const std::filesystem::path& file)
{
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(file, ec);
    if (ec) {
        return "an unknown time";
    }
    return utcText(std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(written))).value_or("an unknown time");
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
 * What the record beside the saved copy says of it.
 */
struct CopyRecord {
    std::string url;                       ///< Where it was fetched from.
    std::optional<std::string> fetchedAt;  ///< When, in UTC. An earlier scrap did not record it.
};

/**
 * The record beside the saved copy: its URL on the first line, and the time
 * it was fetched on the second.
 */
std::optional<CopyRecord> readCopyRecord(const std::filesystem::path& scrapHome)
{
    const auto text = File::readWholeFile(Toolchain::indexCopySourceFile(scrapHome));
    if (! text.has_value()) {
        return std::nullopt;
    }
    const std::size_t urlEnd = text->find('\n');
    if (urlEnd == std::string::npos) {
        return std::nullopt;
    }
    CopyRecord record{ .url = text->substr(0, urlEnd), .fetchedAt = std::nullopt };
    const std::size_t timeEnd = text->find('\n', urlEnd + 1);
    if ((timeEnd != std::string::npos) && (timeEnd > (urlEnd + 1))) {
        record.fetchedAt = text->substr(urlEnd + 1, timeEnd - (urlEnd + 1));
    }
    return record;
}

/**
 * The text of the record beside a copy fetched from @p url at @p now.
 */
std::string copyRecordText(const std::string& url, std::chrono::system_clock::time_point now)
{
    std::string text = url + "\n";
    if (const auto fetchedAt = utcText(now); fetchedAt.has_value()) {
        text += *fetchedAt + "\n";
    }
    return text;
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
    if (! saved->has_value()) {
        return std::unexpected(std::move(failure));
    }
    SavedIndex& copy = **saved;
    return ObtainedIndex{ .index = std::move(copy.index), .standIn = CopyStandingIn{ .fetchedAt = std::move(copy.fetchedAt), .reason = std::move(failure.reason) } };
}

}  // anonymous namespace

std::expected<std::optional<SavedIndex>, IndexFormatUnsupported> readSavedIndex(const std::filesystem::path& scrapHome,
                                                                                const std::string& url,
                                                                                const Toolchain::ToolchainVersion& scrapVersion)
{
    const std::filesystem::path copy = Toolchain::indexCopyFile(scrapHome);
    auto record = readCopyRecord(scrapHome);
    std::error_code ec;
    if ((! std::filesystem::exists(copy, ec)) || (! record.has_value()) || (record->url != url)) {
        return std::nullopt;
    }
    auto index = readIndexFile(copy, scrapVersion);
    if (! index.has_value()) {
        if (index.error().kind == Toolchain::ToolchainIndexErrorKind::UnsupportedFormat) {
            return std::unexpected(IndexFormatUnsupported{ .format = std::move(index.error().message) });
        }
        return std::nullopt;
    }
    std::string fetchedAt = record->fetchedAt.has_value() ? std::move(*record->fetchedAt) : modificationTimeInUtc(copy);
    return SavedIndex{ .index = std::move(*index), .fetchedAt = std::move(fetchedAt) };
}

std::expected<ObtainedIndex, IndexFault> obtainIndex(const std::filesystem::path& scrapHome,
                                                     const IndexSource& source,
                                                     const Toolchain::ToolchainVersion& scrapVersion,
                                                     std::chrono::system_clock::time_point now)
{
    const std::filesystem::path copy = Toolchain::indexCopyFile(scrapHome);
    std::filesystem::path fetched = copy;
    fetched += ".fetched";

    std::error_code ec;
    std::filesystem::create_directories(copy.parent_path(), ec);
    const auto transfer = Fetch::fetchToFile({ .url = source.url, .destination = fetched, .certificateBundle = source.certificateBundle, .sizeLimit = IndexSizeLimit, .headers = {} });
    if (! transfer.has_value()) {
        return fallBackOnCopy(scrapHome, source, scrapVersion, IndexUnavailable{ .reason = Fetch::describeFetchError(transfer.error()), .fetchError = transfer.error() });
    }

    auto index = readIndexFile(fetched, scrapVersion);
    if (! index.has_value()) {
        std::filesystem::remove(fetched, ec);
        if (index.error().kind == Toolchain::ToolchainIndexErrorKind::UnsupportedFormat) {
            return std::unexpected(IndexFormatUnsupported{ .format = std::move(index.error().message) });
        }
        return fallBackOnCopy(scrapHome, source, scrapVersion, IndexUnavailable{ .reason = std::move(index.error().message), .fetchError = std::nullopt });
    }
    // An index that cannot be saved still answers this install. The record goes
    // first, so a run stopped part way leaves a copy that stands in for nothing.
    const std::filesystem::path sourceFile = Toolchain::indexCopySourceFile(scrapHome);
    std::filesystem::remove(sourceFile, ec);
    std::filesystem::rename(fetched, copy, ec);
    if (! ec) {
        ec = File::replaceFile(sourceFile, copyRecordText(source.url, now));
    }
    return ObtainedIndex{ .index = std::move(*index), .standIn = std::nullopt };
}

}  // namespace scrap::Install
