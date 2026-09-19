#include <gtest/gtest.h>

#include "compile/CompilerDriver.h"
#include "project/LanguageStandard.h"
#include "toolchain/CompilerIdentity.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

using namespace scrap::Compile;
using scrap::Project::LanguageStandard;
using scrap::Toolchain::CompilerFamily;
using scrap::Toolchain::CompilerIdentity;

namespace {

/**
 * What one compiler is expected to be given for one standard.
 */
struct Expectation {
    CompilerFamily family;
    int major;
    int minor;
    LanguageStandard standard;
    std::optional<std::string> option;
};

auto describe(const Expectation& expected) -> std::string
{
    static constexpr std::array<const char*, 4> Families{"gcc", "clang", "AppleClang", "unknown"};
    return std::string{Families.at(static_cast<std::size_t>(expected.family))} + ' ' + std::to_string(expected.major) +
        '.' + std::to_string(expected.minor) + " C++" + std::string{scrap::Project::standardNumber(expected.standard)};
}

void expectSpellings(const std::vector<Expectation>& expectations)
{
    for (const Expectation& expected : expectations) {
        const CompilerDriver driver{CompilerIdentity{
            .family = expected.family, .version = {.major = expected.major, .minor = expected.minor, .patch = 0}}};
        EXPECT_EQ(driver.standardOption(expected.standard), expected.option) << describe(expected);
    }
}

}  // namespace

/**
 * A current compiler of each family is given every standard by its own name.
 */
TEST(CompilerDriverTest, NamesEveryStandardForCurrentCompilers)
{
    for (const CompilerIdentity identity :
         {CompilerIdentity{.family = CompilerFamily::Gcc, .version = {.major = 14, .minor = 2, .patch = 0}},
          CompilerIdentity{.family = CompilerFamily::Clang, .version = {.major = 18, .minor = 1, .patch = 3}},
          CompilerIdentity{.family = CompilerFamily::AppleClang, .version = {.major = 16, .minor = 0, .patch = 0}}}) {
        const CompilerDriver driver{identity};
        for (const LanguageStandard standard : scrap::Project::supportedLanguageStandards()) {
            EXPECT_EQ(driver.standardOption(standard), standardNameOption(standard))
                << static_cast<int>(identity.family) << ' ' << scrap::Project::standardNumber(standard);
        }
    }
}

/**
 * gcc takes the draft names before a standard's own, and nothing before
 * those.
 */
TEST(CompilerDriverTest, SpellsTheStandardsGccTakes)
{
    expectSpellings({
        {CompilerFamily::Gcc, 13, 3, LanguageStandard::Cxx26, std::nullopt},
        {CompilerFamily::Gcc, 14, 0, LanguageStandard::Cxx26, "-std=c++26"},
        {CompilerFamily::Gcc, 11, 1, LanguageStandard::Cxx23, "-std=c++23"},
        {CompilerFamily::Gcc, 10, 5, LanguageStandard::Cxx23, std::nullopt},
        {CompilerFamily::Gcc, 10, 5, LanguageStandard::Cxx20, "-std=c++2a"},
        {CompilerFamily::Gcc, 11, 1, LanguageStandard::Cxx20, "-std=c++20"},
        {CompilerFamily::Gcc, 7, 5, LanguageStandard::Cxx17, "-std=c++1z"},
        {CompilerFamily::Gcc, 7, 5, LanguageStandard::Cxx20, std::nullopt},
        {CompilerFamily::Gcc, 4, 8, LanguageStandard::Cxx14, "-std=c++1y"},
        {CompilerFamily::Gcc, 4, 6, LanguageStandard::Cxx11, "-std=c++0x"},
        {CompilerFamily::Gcc, 4, 3, LanguageStandard::Cxx11, std::nullopt},
    });
}

/**
 * clang 12 to 16 take C++23 as c++2b, and clang 17 by its own name.
 */
TEST(CompilerDriverTest, SpellsTheStandardsClangTakes)
{
    expectSpellings({
        {CompilerFamily::Clang, 17, 0, LanguageStandard::Cxx23, "-std=c++23"},
        {CompilerFamily::Clang, 16, 0, LanguageStandard::Cxx23, "-std=c++2b"},
        {CompilerFamily::Clang, 16, 0, LanguageStandard::Cxx26, std::nullopt},
        {CompilerFamily::Clang, 12, 0, LanguageStandard::Cxx23, "-std=c++2b"},
        {CompilerFamily::Clang, 11, 0, LanguageStandard::Cxx23, std::nullopt},
        {CompilerFamily::Clang, 11, 0, LanguageStandard::Cxx20, "-std=c++20"},
        {CompilerFamily::Clang, 10, 0, LanguageStandard::Cxx20, "-std=c++2a"},
        {CompilerFamily::Clang, 4, 0, LanguageStandard::Cxx17, "-std=c++1z"},
    });
}

/**
 * AppleClang is versioned by Xcode: 13 to 15 take C++23 as c++2b, and 16 by
 * its own name.
 */
TEST(CompilerDriverTest, SpellsTheStandardsAppleClangTakes)
{
    expectSpellings({
        {CompilerFamily::AppleClang, 16, 0, LanguageStandard::Cxx23, "-std=c++23"},
        {CompilerFamily::AppleClang, 15, 0, LanguageStandard::Cxx23, "-std=c++2b"},
        {CompilerFamily::AppleClang, 15, 0, LanguageStandard::Cxx26, std::nullopt},
        {CompilerFamily::AppleClang, 13, 0, LanguageStandard::Cxx23, "-std=c++2b"},
        {CompilerFamily::AppleClang, 12, 0, LanguageStandard::Cxx23, std::nullopt},
        {CompilerFamily::AppleClang, 12, 0, LanguageStandard::Cxx20, "-std=c++2a"},
        {CompilerFamily::AppleClang, 9, 0, LanguageStandard::Cxx17, "-std=c++1z"},
    });
}

/**
 * A compiler of unknown family is given each standard by its own name.
 */
TEST(CompilerDriverTest, NamesEveryStandardForAnUnknownCompiler)
{
    const CompilerDriver driver{CompilerIdentity{}};

    for (const LanguageStandard standard : scrap::Project::supportedLanguageStandards()) {
        EXPECT_EQ(driver.standardOption(standard), standardNameOption(standard));
    }
}

/**
 * The known families keep colour in diagnostics read through a pipe; an
 * unknown one is not given an option it might reject.
 */
TEST(CompilerDriverTest, KeepsColorForTheKnownFamilies)
{
    for (const CompilerFamily family : {CompilerFamily::Gcc, CompilerFamily::Clang, CompilerFamily::AppleClang}) {
        const CompilerDriver driver{
            CompilerIdentity{.family = family, .version = {.major = 15, .minor = 0, .patch = 0}}};
        EXPECT_EQ(driver.colorOption(), "-fdiagnostics-color=always") << static_cast<int>(family);
    }
    EXPECT_FALSE(CompilerDriver{CompilerIdentity{}}.colorOption().has_value());
}

/**
 * A standard's own name is -std=c++ followed by its number.
 */
TEST(CompilerDriverTest, NamesAStandardByItsNumber)
{
    EXPECT_EQ(standardNameOption(LanguageStandard::Cxx11), "-std=c++11");
    EXPECT_EQ(standardNameOption(LanguageStandard::Cxx26), "-std=c++26");
}
