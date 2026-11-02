#include "toolchain/InstallRecord.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Toolchain {

namespace {

constexpr std::string_view RuntimeLibraryDirectoriesKey = "runtime-library-dirs";

/**
 * Whether @p path names something inside the directory it is taken against:
 * relative, and with no component that leads up out of it.
 */
bool staysInside(const std::filesystem::path& path)
{
    if (path.empty() || path.has_root_path()) {
        return false;
    }
    return std::ranges::none_of(path, [](const std::filesystem::path& component) {
        return component == "..";
    });
}

/**
 * Why @p node, the value of @p key, is refused, starting with its line.
 */
std::string fault(const toml::node& node, std::string_view key, std::string_view reason)
{
    std::string message = "line " + std::to_string(node.source().begin.line) + ": ";
    message += key;
    message += ": ";
    message += reason;
    return message;
}

/**
 * @p node, the value of @p key, as a path inside the toolchain.
 */
std::expected<std::filesystem::path, std::string> pathInside(const toml::node& node, std::string_view key)
{
    const std::optional<std::string> text = node.value_exact<std::string>();
    if (! text.has_value()) {
        return std::unexpected(fault(node, key, "must be a string"));
    }
    if (! staysInside(*text)) {
        return std::unexpected(fault(node, key, "must be a relative path inside the toolchain"));
    }
    return std::filesystem::path{ *text };
}

/**
 * The runtime library directories @p table records, none when it records
 * no list of them.
 */
std::expected<std::vector<std::filesystem::path>, std::string> runtimeLibraryDirectories(const toml::table& table)
{
    const toml::node* node = table.get(RuntimeLibraryDirectoriesKey);
    if (node == nullptr) {
        return std::vector<std::filesystem::path>{};
    }
    const toml::array* entries = node->as_array();
    if (entries == nullptr) {
        return std::unexpected(fault(*node, RuntimeLibraryDirectoriesKey, "must be an array of strings"));
    }
    std::vector<std::filesystem::path> directories;
    for (const toml::node& entry : *entries) {
        auto directory = pathInside(entry, RuntimeLibraryDirectoriesKey);
        if (! directory.has_value()) {
            return std::unexpected(std::move(directory.error()));
        }
        directories.push_back(std::move(*directory));
    }
    return directories;
}

}  // anonymous namespace

std::expected<InstallRecord, std::string> parseInstallRecord(std::string_view text)
{
    const toml::parse_result parsed = toml::parse(text);
    if (! parsed) {
        const toml::parse_error& error = parsed.error();
        return std::unexpected("line " + std::to_string(error.source().begin.line) + ": " + std::string{ error.description() });
    }
    const toml::node* node = parsed.table().get("compiler");
    if (node == nullptr) {
        return std::unexpected("compiler: is missing");
    }
    auto compiler = pathInside(*node, "compiler");
    if (! compiler.has_value()) {
        return std::unexpected(std::move(compiler.error()));
    }
    auto directories = runtimeLibraryDirectories(parsed.table());
    if (! directories.has_value()) {
        return std::unexpected(std::move(directories.error()));
    }
    return InstallRecord{ .compiler = std::move(*compiler), .runtimeLibraryDirectories = std::move(*directories) };
}

}  // namespace scrap::Toolchain
