#include "toolchain/InstallRecord.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace scrap::Toolchain {

namespace {

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
    const std::optional<std::string> compiler = node->value_exact<std::string>();
    if (! compiler.has_value()) {
        return std::unexpected("line " + std::to_string(node->source().begin.line) + ": compiler: must be a string");
    }
    if (! staysInside(*compiler)) {
        return std::unexpected("line " + std::to_string(node->source().begin.line) + ": compiler: must be a relative path inside the toolchain");
    }
    return InstallRecord{ .compiler = *compiler };
}

}  // namespace scrap::Toolchain
