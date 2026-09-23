#include "toolchain/CompilerIdentity.h"

#include "process/Subprocess.h"

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace scrap::Toolchain {

namespace {

constexpr std::string_view DefinePrefix = "#define ";

/**
 * The value @p macros gives @p name, or nothing when it does not define it.
 */
std::optional<std::string_view> macroValue(std::string_view macros, std::string_view name)
{
    std::size_t start = 0;
    while (start < macros.size()) {
        std::size_t end = macros.find('\n', start);
        if (end == std::string_view::npos) {
            end = macros.size();
        }
        std::string_view line = macros.substr(start, end - start);
        start = end + 1;

        if (! line.starts_with(DefinePrefix)) {
            continue;
        }
        line.remove_prefix(DefinePrefix.size());
        if (line.starts_with(name) && (line.size() > name.size()) && (line[name.size()] == ' ')) {
            return line.substr(name.size() + 1);
        }
    }
    return std::nullopt;
}

/**
 * The number @p macros gives @p name, or 0 when it gives none.
 */
int macroNumber(std::string_view macros, std::string_view name)
{
    const auto value = macroValue(macros, name);
    if (! value.has_value()) {
        return 0;
    }
    int number = 0;
    if (std::from_chars(value->data(), value->data() + value->size(), number).ec != std::errc{}) {
        return 0;
    }
    return number;
}

/**
 * The version spelled by the three macros named.
 */
CompilerVersion versionFrom(std::string_view macros, std::string_view major, std::string_view minor, std::string_view patch)
{
    return CompilerVersion{ .major = macroNumber(macros, major), .minor = macroNumber(macros, minor), .patch = macroNumber(macros, patch) };
}

}  // anonymous namespace

bool isAtLeast(const CompilerVersion& version, const int major, const int minor)
{
    if (version.major != major) {
        return (version.major > major);
    }
    return (version.minor >= minor);
}

CompilerIdentity readCompilerIdentity(std::string_view predefinedMacros)
{
    const auto clangVersion = [predefinedMacros] {
        return versionFrom(predefinedMacros, "__clang_major__", "__clang_minor__", "__clang_patchlevel__");
    };
    if (macroValue(predefinedMacros, "__apple_build_version__").has_value()) {
        return CompilerIdentity{ .family = CompilerFamily::AppleClang, .version = clangVersion() };
    }
    if (macroValue(predefinedMacros, "__clang__").has_value()) {
        return CompilerIdentity{ .family = CompilerFamily::Clang, .version = clangVersion() };
    }
    if (macroValue(predefinedMacros, "__GNUC__").has_value()) {
        return CompilerIdentity{ .family = CompilerFamily::Gcc,
                                 .version = versionFrom(predefinedMacros, "__GNUC__", "__GNUC_MINOR__", "__GNUC_PATCHLEVEL__") };
    }
    return CompilerIdentity{};
}

CompilerIdentity identifyCompiler(const std::filesystem::path& compiler)
{
    const std::vector<std::string> arguments{ compiler.string(), "-dM", "-E", "-x", "c++", "/dev/null" };
    const auto completion = Process::runProgram(arguments, {}, Process::OutputCapture::StandardOutput);
    if ((! completion.has_value()) || (completion->exitCode != 0)) {
        return CompilerIdentity{};
    }
    return readCompilerIdentity(completion->output);
}

}  // namespace scrap::Toolchain
