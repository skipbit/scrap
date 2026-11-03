#include "install/IndexCopy.h"

#include "fetch/FetchErrorText.h"
#include "fetch/UrlFetch.h"
#include "file/WholeFile.h"
#include "toolchain/ScrapHomeLayout.h"
#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainIndexParser.h"
#include "toolchain/ToolchainVersion.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
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
 * writes the copy, so this is when it was fetched.
 */
std::string modificationTimeInUtc(const std::filesystem::path& file)
{
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(file, ec);
    if (ec) {
        return "an unknown time";
    }
    const std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(std::chrono::file_clock::to_sys(written)));
    std::tm parts{};
    std::array<char, 32> text{};
    if ((::gmtime_r(&seconds, &parts) == nullptr) || (std::strftime(text.data(), text.size(), "%Y-%m-%dT%H:%M:%SZ", &parts) == 0)) {
        return "an unknown time";
    }
    return std::string{ text.data() };
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
 * The index the saved copy holds, standing in for one that could not be
 * obtained for @p failure.
 */
std::expected<ObtainedIndex, IndexFault> fallBackOnCopy(const std::filesystem::path& copy, const Toolchain::ToolchainVersion& scrapVersion, IndexUnavailable failure)
{
    std::error_code ec;
    if (! std::filesystem::exists(copy, ec)) {
        return std::unexpected(std::move(failure));
    }
    auto index = readIndexFile(copy, scrapVersion);
    if (! index.has_value()) {
        if (index.error().kind == Toolchain::ToolchainIndexErrorKind::UnsupportedFormat) {
            return std::unexpected(IndexFormatUnsupported{ .format = std::move(index.error().message) });
        }
        return std::unexpected(std::move(failure));
    }
    return ObtainedIndex{ .index = std::move(*index), .copyFetchedAt = modificationTimeInUtc(copy) };
}

}  // anonymous namespace

std::expected<ObtainedIndex, IndexFault> obtainIndex(const std::filesystem::path& scrapHome, const IndexSource& source, const Toolchain::ToolchainVersion& scrapVersion)
{
    const std::filesystem::path copy = Toolchain::indexCopyFile(scrapHome);
    std::filesystem::path fetched = copy;
    fetched += ".fetched";

    std::error_code ec;
    std::filesystem::create_directories(copy.parent_path(), ec);
    const auto transfer = Fetch::fetchToFile({ .url = source.url, .destination = fetched, .certificateBundle = source.certificateBundle, .sizeLimit = IndexSizeLimit });
    if (! transfer.has_value()) {
        return fallBackOnCopy(copy, scrapVersion, IndexUnavailable{ .reason = Fetch::describeFetchError(transfer.error()), .fetchError = transfer.error() });
    }

    auto index = readIndexFile(fetched, scrapVersion);
    if (! index.has_value()) {
        std::filesystem::remove(fetched, ec);
        if (index.error().kind == Toolchain::ToolchainIndexErrorKind::UnsupportedFormat) {
            return std::unexpected(IndexFormatUnsupported{ .format = std::move(index.error().message) });
        }
        return fallBackOnCopy(copy, scrapVersion, IndexUnavailable{ .reason = std::move(index.error().message), .fetchError = std::nullopt });
    }
    // An index that cannot be saved still answers this install.
    std::filesystem::rename(fetched, copy, ec);
    return ObtainedIndex{ .index = std::move(*index), .copyFetchedAt = std::nullopt };
}

}  // namespace scrap::Install
