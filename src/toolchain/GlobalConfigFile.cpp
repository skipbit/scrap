#include "toolchain/GlobalConfigFile.h"

#include "file/WholeFile.h"
#include "toolchain/GlobalConfig.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace scrap::Toolchain {

namespace {

/**
 * The text of @p file, empty when nothing is there.
 */
std::expected<std::string, GlobalConfigFileError> readText(const std::filesystem::path& file)
{
    std::error_code ec;
    if (! std::filesystem::exists(file, ec) && ! ec) {
        return std::string{};
    }
    auto text = File::readWholeFile(file);
    if (! text.has_value()) {
        return std::unexpected(CannotReadConfig{ .path = file });
    }
    return std::move(*text);
}

}  // anonymous namespace

std::expected<std::optional<std::string>, GlobalConfigFileError> readGlobalDefaultFile(const std::filesystem::path& file)
{
    auto text = readText(file);
    if (! text.has_value()) {
        return std::unexpected(std::move(text.error()));
    }
    auto toolchain = readDefaultToolchain(*text);
    if (! toolchain.has_value()) {
        return std::unexpected(std::move(toolchain.error()));
    }
    return *toolchain;
}

std::expected<void, GlobalConfigFileError> writeGlobalDefaultFile(const std::filesystem::path& file, const std::optional<std::string>& toolchain)
{
    auto text = readText(file);
    if (! text.has_value()) {
        return std::unexpected(std::move(text.error()));
    }
    auto written = writeDefaultToolchain(*text, toolchain);
    if (! written.has_value()) {
        return std::unexpected(std::move(written.error()));
    }
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) {
        return std::unexpected(CannotCreateDirectory{ .path = file.parent_path(), .code = ec });
    }
    ec = File::replaceFile(file, *written);
    if (ec) {
        return std::unexpected(CannotWriteConfig{ .path = file, .code = ec });
    }
    return {};
}

}  // namespace scrap::Toolchain
