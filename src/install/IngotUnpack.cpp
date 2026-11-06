#include "install/IngotUnpack.h"

#include "tar/TarExtractor.h"
#include "tar/TarReader.h"
#include "xz/XzFile.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

namespace scrap::Install {

namespace {

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

}  // anonymous namespace

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

}  // namespace scrap::Install
