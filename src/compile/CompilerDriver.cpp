#include "compile/CompilerDriver.h"

#include "project/LanguageStandard.h"
#include "toolchain/CompilerIdentity.h"

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Compile {

namespace {

using Project::CStandard;
using Project::LanguageStandard;
using Toolchain::CompilerFamily;

/**
 * An option a compiler takes for a standard from the version given on.
 */
template <typename Standard>
struct Spelling {
    Standard standard;
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string_view option;
};

// The tables follow CMake 4.4.3 (Modules/Compiler/GNU.cmake, Clang.cmake and
// AppleClang-CXX.cmake). The spellings of one standard are listed newest
// first, and a version takes the first it reaches.

constexpr std::array GccSpellings{
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx26, .major = 14, .minor = 0, .option = "-std=c++26" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx23, .major = 11, .minor = 1, .option = "-std=c++23" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx20, .major = 11, .minor = 1, .option = "-std=c++20" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx20, .major = 8, .minor = 0, .option = "-std=c++2a" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx17, .major = 8, .minor = 0, .option = "-std=c++17" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx17, .major = 5, .minor = 1, .option = "-std=c++1z" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx14, .major = 4, .minor = 9, .option = "-std=c++14" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx14, .major = 4, .minor = 8, .option = "-std=c++1y" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx11, .major = 4, .minor = 7, .option = "-std=c++11" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx11, .major = 4, .minor = 4, .option = "-std=c++0x" },
};

constexpr std::array ClangSpellings{
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx26, .major = 17, .minor = 0, .option = "-std=c++26" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx23, .major = 17, .minor = 0, .option = "-std=c++23" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx23, .major = 12, .minor = 0, .option = "-std=c++2b" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx20, .major = 11, .minor = 0, .option = "-std=c++20" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx20, .major = 5, .minor = 0, .option = "-std=c++2a" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx17, .major = 5, .minor = 0, .option = "-std=c++17" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx17, .major = 3, .minor = 5, .option = "-std=c++1z" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx14, .major = 3, .minor = 5, .option = "-std=c++14" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx14, .major = 3, .minor = 4, .option = "-std=c++1y" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx11, .major = 3, .minor = 1, .option = "-std=c++11" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx11, .major = 2, .minor = 1, .option = "-std=c++0x" },
};

constexpr std::array AppleClangSpellings{
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx26, .major = 16, .minor = 0, .option = "-std=c++26" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx23, .major = 16, .minor = 0, .option = "-std=c++23" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx23, .major = 13, .minor = 0, .option = "-std=c++2b" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx20, .major = 13, .minor = 0, .option = "-std=c++20" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx20, .major = 10, .minor = 0, .option = "-std=c++2a" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx17, .major = 10, .minor = 0, .option = "-std=c++17" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx17, .major = 6, .minor = 1, .option = "-std=c++1z" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx14, .major = 6, .minor = 1, .option = "-std=c++14" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx14, .major = 5, .minor = 1, .option = "-std=c++1y" },
    Spelling<LanguageStandard>{ .standard = LanguageStandard::Cxx11, .major = 4, .minor = 0, .option = "-std=c++11" },
};

// The C tables follow CMake 4.4.3 (Modules/Compiler/GNU.cmake, Clang-C.cmake
// and AppleClang-C.cmake) in the same way.

constexpr std::array GccCSpellings{
    Spelling<CStandard>{ .standard = CStandard::C23, .major = 9, .minor = 1, .option = "-std=c2x" },
    Spelling<CStandard>{ .standard = CStandard::C17, .major = 8, .minor = 1, .option = "-std=c17" },
    Spelling<CStandard>{ .standard = CStandard::C11, .major = 4, .minor = 7, .option = "-std=c11" },
    Spelling<CStandard>{ .standard = CStandard::C11, .major = 4, .minor = 6, .option = "-std=c1x" },
    Spelling<CStandard>{ .standard = CStandard::C99, .major = 3, .minor = 4, .option = "-std=c99" },
};

constexpr std::array ClangCSpellings{
    Spelling<CStandard>{ .standard = CStandard::C23, .major = 18, .minor = 0, .option = "-std=c23" },
    Spelling<CStandard>{ .standard = CStandard::C23, .major = 9, .minor = 0, .option = "-std=c2x" },
    Spelling<CStandard>{ .standard = CStandard::C17, .major = 6, .minor = 0, .option = "-std=c17" },
    Spelling<CStandard>{ .standard = CStandard::C11, .major = 3, .minor = 1, .option = "-std=c11" },
    Spelling<CStandard>{ .standard = CStandard::C11, .major = 3, .minor = 0, .option = "-std=c1x" },
    Spelling<CStandard>{ .standard = CStandard::C99, .major = 2, .minor = 1, .option = "-std=c99" },
};

constexpr std::array AppleClangCSpellings{
    Spelling<CStandard>{ .standard = CStandard::C23, .major = 11, .minor = 0, .patch = 3, .option = "-std=c2x" },
    Spelling<CStandard>{ .standard = CStandard::C17, .major = 11, .minor = 0, .option = "-std=c17" },
    Spelling<CStandard>{ .standard = CStandard::C11, .major = 4, .minor = 0, .option = "-std=c11" },
    Spelling<CStandard>{ .standard = CStandard::C99, .major = 4, .minor = 0, .option = "-std=c99" },
};

/// Keeps colour in diagnostics read through a pipe; gcc and clang both take it.
constexpr std::string_view ColorOption = "-fdiagnostics-color=always";

/**
 * The option @p spellings give @p standard at @p version, or nothing when the
 * version predates them all.
 */
template <typename Standard>
std::optional<std::string> lookUp(const std::span<const Spelling<Standard>> spellings, const Standard standard, const Toolchain::CompilerVersion& version)
{
    for (const auto& spelling : spellings) {
        if ((spelling.standard == standard) && Toolchain::isAtLeast(version, spelling.major, spelling.minor, spelling.patch)) {
            return std::string{ spelling.option };
        }
    }
    return std::nullopt;
}

/**
 * The option the compiler @p identity takes for @p standard, from the table of
 * its family.
 */
template <typename Standard>
std::optional<std::string> standardOptionFrom(
    const std::span<const Spelling<Standard>> gcc,
    const std::span<const Spelling<Standard>> clang,
    const std::span<const Spelling<Standard>> appleClang,
    const Standard standard,
    const Toolchain::CompilerIdentity& identity)
{
    switch (identity.family) {
    case CompilerFamily::Gcc:
        return lookUp(gcc, standard, identity.version);
    case CompilerFamily::Clang:
        return lookUp(clang, standard, identity.version);
    case CompilerFamily::AppleClang:
        return lookUp(appleClang, standard, identity.version);
    case CompilerFamily::Unknown:
        return standardNameOption(standard);
    }
    std::unreachable();
}

}  // anonymous namespace

CompilerDriver::CompilerDriver(Toolchain::CompilerIdentity identity)
    : _identity(identity)
{
}

std::optional<std::string> CompilerDriver::standardOption(const LanguageStandard standard) const
{
    return standardOptionFrom<LanguageStandard>(GccSpellings, ClangSpellings, AppleClangSpellings, standard, _identity);
}

std::optional<std::string> CompilerDriver::standardOption(const CStandard standard) const
{
    return standardOptionFrom<CStandard>(GccCSpellings, ClangCSpellings, AppleClangCSpellings, standard, _identity);
}

std::optional<std::string> CompilerDriver::colorOption() const
{
    if (_identity.family == CompilerFamily::Unknown) {
        return std::nullopt;
    }
    return std::string{ ColorOption };
}

std::optional<std::vector<std::string>> CompilerDriver::dependencyFileOptions(const std::filesystem::path& file) const
{
    if (_identity.family == CompilerFamily::Unknown) {
        return std::nullopt;
    }
    return std::vector<std::string>{ "-MD", "-MF", file.string() };
}

std::string standardNameOption(const LanguageStandard standard)
{
    return "-std=c++" + std::string{ Project::standardNumber(standard) };
}

std::string standardNameOption(const CStandard standard)
{
    return "-std=c" + std::string{ Project::standardNumber(standard) };
}

}  // namespace scrap::Compile
