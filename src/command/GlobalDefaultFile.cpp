#include "command/GlobalDefaultFile.h"

#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "file/WholeFile.h"
#include "toolchain/GlobalConfig.h"

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace scrap::Command {

namespace {

constexpr std::string_view PermissionHint = "hint: check the permissions of the path\n";

/**
 * A fault in the text of @p file, pointed at as a compiler points at one.
 */
std::string renderFault(const std::filesystem::path& file, const Toolchain::GlobalConfigFault& fault)
{
    std::string text = printablePath(file) + ":" + std::to_string(fault.line) + ":" + std::to_string(fault.column) + ": error: ";
    if (! fault.key.empty()) {
        text += fault.key + ": ";
    }
    text += printableText(fault.message);
    text += "\nhint: fix the file, or remove it to clear the settings it holds\n";
    return text;
}

/**
 * The text of @p file, empty when nothing is there.
 */
std::expected<std::string, std::string> readText(const std::filesystem::path& file)
{
    std::error_code ec;
    if (! std::filesystem::exists(file, ec) && ! ec) {
        return std::string{};
    }
    auto text = File::readWholeFile(file);
    if (! text.has_value()) {
        return std::unexpected("error: cannot read '" + printablePath(file) + "'\n" + std::string{ PermissionHint });
    }
    return std::move(*text);
}

}  // anonymous namespace

std::expected<std::filesystem::path, std::string> requireScrapHome(const RuntimeEnvironment& env)
{
    if (! env.scrapHome.has_value()) {
        return std::unexpected(std::string{ "error: cannot tell where to keep toolchains: SCRAP_HOME and HOME are both unset\n"
                                            "hint: set SCRAP_HOME to the directory to keep them in, and run the command again\n" });
    }
    return *env.scrapHome;
}

std::filesystem::path globalConfigFile(const std::filesystem::path& scrapHome)
{
    return scrapHome / "config.toml";
}

std::expected<std::optional<std::string>, std::string> readGlobalDefault(const std::filesystem::path& file)
{
    auto text = readText(file);
    if (! text.has_value()) {
        return std::unexpected(text.error());
    }
    auto toolchain = Toolchain::readDefaultToolchain(*text);
    if (! toolchain.has_value()) {
        return std::unexpected(renderFault(file, toolchain.error()));
    }
    return *toolchain;
}

std::expected<void, std::string> writeGlobalDefault(const std::filesystem::path& file, const std::optional<std::string>& toolchain)
{
    auto text = readText(file);
    if (! text.has_value()) {
        return std::unexpected(text.error());
    }
    auto written = Toolchain::writeDefaultToolchain(*text, toolchain);
    if (! written.has_value()) {
        return std::unexpected(renderFault(file, written.error()));
    }
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) {
        return std::unexpected("error: cannot create '" + printablePath(file.parent_path()) + "': " + ec.message() + "\n" + std::string{ PermissionHint });
    }
    ec = File::replaceFile(file, *written);
    if (ec) {
        return std::unexpected("error: cannot write '" + printablePath(file) + "': " + ec.message() + "\n" + std::string{ PermissionHint });
    }
    return {};
}

}  // namespace scrap::Command
