#include "compile/CompilerDriver.h"

#include "project/LanguageStandard.h"
#include "toolchain/CompilerIdentity.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace scrap::Compile {

namespace {

using Project::LanguageStandard;
using Toolchain::CompilerFamily;

/**
 * An option a compiler takes for a standard from the version given on.
 */
struct Spelling {
    LanguageStandard standard;
    int major;
    int minor;
    std::string_view option;
};

// The tables follow CMake 4.4.3 (Modules/Compiler/GNU.cmake, Clang.cmake and
// AppleClang-CXX.cmake). The spellings of one standard are listed newest
// first, and a version takes the first it reaches.

constexpr std::array GccSpellings{
    Spelling{ .standard = LanguageStandard::Cxx26, .major = 14, .minor = 0, .option = "-std=c++26" },
    Spelling{ .standard = LanguageStandard::Cxx23, .major = 11, .minor = 1, .option = "-std=c++23" },
    Spelling{ .standard = LanguageStandard::Cxx20, .major = 11, .minor = 1, .option = "-std=c++20" },
    Spelling{ .standard = LanguageStandard::Cxx20, .major = 8, .minor = 0, .option = "-std=c++2a" },
    Spelling{ .standard = LanguageStandard::Cxx17, .major = 8, .minor = 0, .option = "-std=c++17" },
    Spelling{ .standard = LanguageStandard::Cxx17, .major = 5, .minor = 1, .option = "-std=c++1z" },
    Spelling{ .standard = LanguageStandard::Cxx14, .major = 4, .minor = 9, .option = "-std=c++14" },
    Spelling{ .standard = LanguageStandard::Cxx14, .major = 4, .minor = 8, .option = "-std=c++1y" },
    Spelling{ .standard = LanguageStandard::Cxx11, .major = 4, .minor = 7, .option = "-std=c++11" },
    Spelling{ .standard = LanguageStandard::Cxx11, .major = 4, .minor = 4, .option = "-std=c++0x" },
};

constexpr std::array ClangSpellings{
    Spelling{ .standard = LanguageStandard::Cxx26, .major = 17, .minor = 0, .option = "-std=c++26" },
    Spelling{ .standard = LanguageStandard::Cxx23, .major = 17, .minor = 0, .option = "-std=c++23" },
    Spelling{ .standard = LanguageStandard::Cxx23, .major = 12, .minor = 0, .option = "-std=c++2b" },
    Spelling{ .standard = LanguageStandard::Cxx20, .major = 11, .minor = 0, .option = "-std=c++20" },
    Spelling{ .standard = LanguageStandard::Cxx20, .major = 5, .minor = 0, .option = "-std=c++2a" },
    Spelling{ .standard = LanguageStandard::Cxx17, .major = 5, .minor = 0, .option = "-std=c++17" },
    Spelling{ .standard = LanguageStandard::Cxx17, .major = 3, .minor = 5, .option = "-std=c++1z" },
    Spelling{ .standard = LanguageStandard::Cxx14, .major = 3, .minor = 5, .option = "-std=c++14" },
    Spelling{ .standard = LanguageStandard::Cxx14, .major = 3, .minor = 4, .option = "-std=c++1y" },
    Spelling{ .standard = LanguageStandard::Cxx11, .major = 3, .minor = 1, .option = "-std=c++11" },
    Spelling{ .standard = LanguageStandard::Cxx11, .major = 2, .minor = 1, .option = "-std=c++0x" },
};

constexpr std::array AppleClangSpellings{
    Spelling{ .standard = LanguageStandard::Cxx26, .major = 16, .minor = 0, .option = "-std=c++26" },
    Spelling{ .standard = LanguageStandard::Cxx23, .major = 16, .minor = 0, .option = "-std=c++23" },
    Spelling{ .standard = LanguageStandard::Cxx23, .major = 13, .minor = 0, .option = "-std=c++2b" },
    Spelling{ .standard = LanguageStandard::Cxx20, .major = 13, .minor = 0, .option = "-std=c++20" },
    Spelling{ .standard = LanguageStandard::Cxx20, .major = 10, .minor = 0, .option = "-std=c++2a" },
    Spelling{ .standard = LanguageStandard::Cxx17, .major = 10, .minor = 0, .option = "-std=c++17" },
    Spelling{ .standard = LanguageStandard::Cxx17, .major = 6, .minor = 1, .option = "-std=c++1z" },
    Spelling{ .standard = LanguageStandard::Cxx14, .major = 6, .minor = 1, .option = "-std=c++14" },
    Spelling{ .standard = LanguageStandard::Cxx14, .major = 5, .minor = 1, .option = "-std=c++1y" },
    Spelling{ .standard = LanguageStandard::Cxx11, .major = 4, .minor = 0, .option = "-std=c++11" },
};

/// Keeps colour in diagnostics read through a pipe; gcc and clang both take it.
constexpr std::string_view ColorOption = "-fdiagnostics-color=always";

/**
 * The option @p spellings give @p standard at @p version, or nothing when the
 * version predates them all.
 */
auto lookUp(std::span<const Spelling> spellings,
            const LanguageStandard standard,
            const Toolchain::CompilerVersion& version) -> std::optional<std::string>
{
    for (const Spelling& spelling : spellings) {
        if ((spelling.standard == standard) && Toolchain::isAtLeast(version, spelling.major, spelling.minor)) {
            return std::string{ spelling.option };
        }
    }
    return std::nullopt;
}

}  // anonymous namespace

CompilerDriver::CompilerDriver(Toolchain::CompilerIdentity identity)
    : _identity(identity)
{
}

auto CompilerDriver::standardOption(const LanguageStandard standard) const -> std::optional<std::string>
{
    switch (_identity.family) {
    case CompilerFamily::Gcc:
        return lookUp(GccSpellings, standard, _identity.version);
    case CompilerFamily::Clang:
        return lookUp(ClangSpellings, standard, _identity.version);
    case CompilerFamily::AppleClang:
        return lookUp(AppleClangSpellings, standard, _identity.version);
    case CompilerFamily::Unknown:
        return standardNameOption(standard);
    }
    std::unreachable();
}

auto CompilerDriver::colorOption() const -> std::optional<std::string>
{
    if (_identity.family == CompilerFamily::Unknown) {
        return std::nullopt;
    }
    return std::string{ ColorOption };
}

auto standardNameOption(const LanguageStandard standard) -> std::string
{
    return "-std=c++" + std::string{ Project::standardNumber(standard) };
}

}  // namespace scrap::Compile
