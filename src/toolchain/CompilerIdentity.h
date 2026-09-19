#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace scrap::Toolchain {

/**
 * @brief The kind of compiler, as far as it matters to the flags it takes.
 */
enum class CompilerFamily : std::uint8_t {
    Gcc,
    Clang,
    AppleClang,  ///< Versioned by Xcode, not by the LLVM release it derives from.
    Unknown      ///< None of the above, or a compiler that could not be asked.
};

/**
 * @brief The version a compiler reports of itself.
 */
struct CompilerVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

/**
 * @brief Which compiler a program is, and which version.
 */
struct CompilerIdentity {
    CompilerFamily family = CompilerFamily::Unknown;
    CompilerVersion version;  ///< Meaningful unless the family is Unknown.
};

/**
 * @brief Whether @p version is @p major.@p minor or later.
 */
[[nodiscard]] auto isAtLeast(const CompilerVersion& version, int major, int minor = 0) -> bool;

/**
 * @brief Tell which compiler wrote @p predefinedMacros.
 *
 * The macros are read in the order that tells the families apart: AppleClang
 * defines what clang does, and clang defines __GNUC__ as well.
 *
 * @param predefinedMacros What the compiler prints for -dM -E: one
 *        "#define NAME VALUE" line per macro.
 */
[[nodiscard]] auto readCompilerIdentity(std::string_view predefinedMacros) -> CompilerIdentity;

/**
 * @brief Ask @p compiler which compiler it is.
 *
 * The compiler is run once to print the macros it predefines. One that cannot
 * be run, fails, or defines none of the macros read is Unknown rather than an
 * error: it may still compile, and whether it does is the build's to report.
 */
[[nodiscard]] auto identifyCompiler(const std::filesystem::path& compiler) -> CompilerIdentity;

}  // namespace scrap::Toolchain
