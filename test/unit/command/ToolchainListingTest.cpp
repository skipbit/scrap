#include "command/ToolchainListing.h"

#include "command/ToolchainNaming.h"
#include "toolchain/InstalledToolchain.h"
#include "toolchain/SystemCompiler.h"
#include "toolchain/ToolchainChoice.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace scrap::Command;
using scrap::Toolchain::CompilerOrigin;
using scrap::Toolchain::InstalledName;
using scrap::Toolchain::parseToolchainChoice;
using scrap::Toolchain::SystemCompiler;
using scrap::Toolchain::ToolchainVersion;

namespace {

const SystemCompiler FoundCompiler{ .path = "/usr/bin/c++", .origin = CompilerOrigin::DefaultOnPath };

InstalledName installed(const std::string& family, std::string_view version)
{
    return InstalledName{ .family = family, .version = ToolchainVersion::parse(version).value() };
}

NamedToolchain named(const std::string& text, const ChoiceSource source)
{
    return NamedToolchain{ .text = text, .choice = parseToolchainChoice(text).value(), .source = source };
}

/**
 * @p reason as a test reads it.
 */
std::string describe(const UseReason& reason)
{
    if (const auto* source = std::get_if<ChoiceSource>(&reason)) {
        return std::string{ describeSource(*source) };
    }
    return std::string{ describeOrigin(std::get<CompilerOrigin>(reason)) };
}

/**
 * Each toolchain of @p listing on a line of its own: its name, then what is
 * marked on it.
 */
std::vector<std::string> rowsOf(const ToolchainListing& listing)
{
    std::vector<std::string> rows;
    for (const ListedToolchain& each : listing.toolchains) {
        std::string row = each.name;
        if (each.compiler.has_value()) {
            row += " at " + each.compiler->string();
        }
        if (! each.present) {
            row += ", not there";
        }
        if (each.isDefault) {
            row += ", default";
        }
        if (each.usedHere.has_value()) {
            row += ", used here " + describe(*each.usedHere);
        }
        rows.push_back(row);
    }
    return rows;
}

}  // namespace

/**
 * The global default and the toolchain the project names are marked on
 * their own rows.
 */
TEST(ToolchainListingTest, MarksTheDefaultAndTheOneTheProjectUses)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "15.2.0"), installed("gcc", "16.2.0") },
                                          .systemCompiler = std::nullopt,
                                          .globalDefault = named("gcc@16.2.0", ChoiceSource::GlobalDefault),
                                          .here = named("gcc@15.2.0", ChoiceSource::Project) });

    EXPECT_TRUE(listing.inProject);
    EXPECT_EQ(rowsOf(listing), (std::vector<std::string>{ "gcc@15.2.0, used here from scrap.toml", "gcc@16.2.0, default" }));
}

/**
 * Outside a project nothing is used here, and the default alone is marked.
 */
TEST(ToolchainListingTest, MarksTheDefaultAloneOutsideAProject)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "15.2.0"), installed("gcc", "16.2.0") },
                                          .systemCompiler = FoundCompiler,
                                          .globalDefault = named("gcc@16.2.0", ChoiceSource::GlobalDefault),
                                          .here = std::nullopt });

    EXPECT_FALSE(listing.inProject);
    EXPECT_EQ(rowsOf(listing), (std::vector<std::string>{ "system at /usr/bin/c++", "gcc@15.2.0", "gcc@16.2.0, default" }));
}

/**
 * The system's compiler is listed first when it is found, and is used here
 * when nothing names a toolchain, as it was found.
 */
TEST(ToolchainListingTest, UsesTheSystemCompilerWhenNothingIsNamed)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "15.2.0") },
                                          .systemCompiler = SystemCompiler{ .path = "/opt/bin/clang++", .origin = CompilerOrigin::CompilerVariable },
                                          .globalDefault = std::nullopt,
                                          .here = NothingNamedHere{} });

    EXPECT_EQ(rowsOf(listing), (std::vector<std::string>{ "system at /opt/bin/clang++, used here from CXX", "gcc@15.2.0" }));
}

/**
 * With nothing named and no system compiler, nothing is used here.
 */
TEST(ToolchainListingTest, UsesNothingWhenNothingIsNamedAndTheSystemHasNoCompiler)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "15.2.0") }, .systemCompiler = std::nullopt, .globalDefault = std::nullopt, .here = NothingNamedHere{} });

    EXPECT_EQ(rowsOf(listing), (std::vector<std::string>{ "gcc@15.2.0" }));
}

/**
 * A specifier marks the newest installed version it names, as a build uses
 * it.
 */
TEST(ToolchainListingTest, MarksTheNewestVersionASpecifierNames)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "15.1.0"), installed("gcc", "15.2.0"), installed("gcc", "16.2.0") },
                                          .systemCompiler = std::nullopt,
                                          .globalDefault = named("gcc", ChoiceSource::GlobalDefault),
                                          .here = named("gcc@15", ChoiceSource::VariableOverProject) });

    EXPECT_EQ(rowsOf(listing),
              (std::vector<std::string>{ "gcc@15.1.0", "gcc@15.2.0, used here from SCRAP_TOOLCHAIN, overriding scrap.toml", "gcc@16.2.0, default" }));
}

/**
 * A name that is not installed is added at the end as written, once even
 * when the default names it too.
 */
TEST(ToolchainListingTest, AddsANamedToolchainThatIsNotInstalled)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "16.2.0") },
                                          .systemCompiler = std::nullopt,
                                          .globalDefault = named("gcc@15.2.0", ChoiceSource::GlobalDefault),
                                          .here = named("gcc@15.2.0", ChoiceSource::Variable) });

    EXPECT_EQ(rowsOf(listing), (std::vector<std::string>{ "gcc@16.2.0", "gcc@15.2.0, not there, default, used here from SCRAP_TOOLCHAIN" }));
}

/**
 * system named where the system has no compiler is added at the end as not
 * there; where it has one, its row is marked.
 */
TEST(ToolchainListingTest, MarksSystemWhereverItIsNamed)
{
    const auto missing = listToolchains({ .installed = { installed("gcc", "16.2.0") },
                                          .systemCompiler = std::nullopt,
                                          .globalDefault = std::nullopt,
                                          .here = named("system", ChoiceSource::Project) });
    EXPECT_EQ(rowsOf(missing), (std::vector<std::string>{ "gcc@16.2.0", "system, not there, used here from scrap.toml" }));

    const auto found = listToolchains({ .installed = {}, .systemCompiler = FoundCompiler, .globalDefault = named("system", ChoiceSource::GlobalDefault), .here = std::nullopt });
    EXPECT_EQ(rowsOf(found), (std::vector<std::string>{ "system at /usr/bin/c++, default" }));
}

/**
 * When what names the toolchain here cannot be read, nothing is marked as
 * used, not even the system's compiler.
 */
TEST(ToolchainListingTest, MarksNothingUsedWhenTheNameCannotBeRead)
{
    const auto listing = listToolchains({ .installed = { installed("gcc", "16.2.0") },
                                          .systemCompiler = FoundCompiler,
                                          .globalDefault = named("gcc@16.2.0", ChoiceSource::GlobalDefault),
                                          .here = NameHereUnreadable{} });

    EXPECT_TRUE(listing.inProject);
    EXPECT_EQ(rowsOf(listing), (std::vector<std::string>{ "system at /usr/bin/c++", "gcc@16.2.0, default" }));
}
