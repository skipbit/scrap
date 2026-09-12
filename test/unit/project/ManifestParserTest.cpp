#include <gtest/gtest.h>

#include "project/Manifest.h"
#include "project/ManifestError.h"
#include "project/ManifestParser.h"

#include "TempDirectory.h"

#include <string>
#include <string_view>

using namespace scrap::Project;
using scrap::TestSupport::TempDirectory;

namespace {

constexpr std::string_view ManifestName = "scrap.toml";

}  // namespace

/**
 * A complete manifest yields the package fields and the declared targets.
 */
TEST(ManifestParserTest, ParsesPackageAndDeclaredTargets)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"
std = "20"

[[bin]]
name = "my-app"
src = "src/main.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->package.name, "my-app");
    EXPECT_EQ(manifest->package.version, "0.1.0");
    EXPECT_EQ(manifest->package.standard, "20");
    ASSERT_EQ(manifest->targets.size(), 1);
    EXPECT_EQ(manifest->targets[0].kind, TargetKind::Executable);
    EXPECT_EQ(manifest->targets[0].name, "my-app");
    EXPECT_EQ(manifest->targets[0].entryPoint, "src/main.cpp");
}

/**
 * package.std is optional and defaults to the standard the project targets.
 */
TEST(ManifestParserTest, DefaultsStandardWhenAbsent)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->package.standard, "23");
}

/**
 * A manifest that declares no targets parses, leaving the inference to the
 * caller rather than guessing here.
 */
TEST(ManifestParserTest, LeavesTargetsEmptyWhenNoneDeclared)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->targets.empty());
    EXPECT_FALSE(manifest->declaresTargets);
}

/**
 * [[lib]] entries become library targets, alongside any executables.
 */
TEST(ManifestParserTest, ParsesLibraryTargets)
{
    constexpr std::string_view text = R"(
[package]
name = "my-lib"
version = "0.1.0"

[[lib]]
name = "my-lib"
src = "src/lib.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    ASSERT_EQ(manifest->targets.size(), 1);
    EXPECT_EQ(manifest->targets[0].kind, TargetKind::Library);
    EXPECT_EQ(manifest->targets[0].entryPoint, "src/lib.cpp");
}

/**
 * Tables reserved for later versions are accepted and left unread, so a
 * manifest that declares them still loads.
 */
TEST(ManifestParserTest, AcceptsTablesReservedForLaterVersions)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[dependencies]
fmt = "10.2.1"

[toolchain]
name = "llvm"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->package.name, "my-app");
}

/**
 * A missing [package] table is reported against the file, since there is no
 * position in it to point at.
 */
TEST(ManifestParserTest, ReportsMissingPackageTable)
{
    constexpr std::string_view text = "[[bin]]\nname = \"a\"\nsrc = \"src/main.cpp\"\n";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package");
    EXPECT_FALSE(manifest.error().position.has_value());
    EXPECT_EQ(describe(manifest.error()), "scrap.toml: package: required table is missing");
}

/**
 * A missing required key points at the table it belongs to.
 */
TEST(ManifestParserTest, ReportsMissingRequiredKeyAtItsTable)
{
    constexpr std::string_view text = "[package]\nversion = \"0.1.0\"\n";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package.name");
    ASSERT_TRUE(manifest.error().position.has_value());
    EXPECT_EQ(manifest.error().position->line, 1);
    EXPECT_EQ(manifest.error().position->column, 1);
    EXPECT_EQ(describe(manifest.error()), "scrap.toml:1:1: package.name: required key is missing");
}

/**
 * A value of the wrong type points at the value itself.
 */
TEST(ManifestParserTest, ReportsWrongTypeAtTheValue)
{
    constexpr std::string_view text = "[package]\nname = \"a\"\nversion = 1\n";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package.version");
    ASSERT_TRUE(manifest.error().position.has_value());
    EXPECT_EQ(manifest.error().position->line, 3);
    EXPECT_EQ(manifest.error().position->column, 11);
    EXPECT_EQ(describe(manifest.error()), "scrap.toml:3:11: package.version: must be a string");
}

/**
 * An empty required value is rejected rather than accepted as a name.
 */
TEST(ManifestParserTest, ReportsEmptyRequiredValue)
{
    constexpr std::string_view text = "[package]\nname = \"\"\nversion = \"0.1.0\"\n";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package.name");
    EXPECT_EQ(manifest.error().message, "must not be empty");
}

/**
 * A syntax error carries the position the parser stopped at and no key.
 */
TEST(ManifestParserTest, ReportsSyntaxErrorWithPosition)
{
    constexpr std::string_view text = "[package]\nname = \n";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_TRUE(manifest.error().key.empty());
    ASSERT_TRUE(manifest.error().position.has_value());
    EXPECT_EQ(manifest.error().position->line, 2);
    // The exact column is toml++'s to decide; what this pins is that a real
    // position is carried through rather than a default-constructed one.
    EXPECT_GT(manifest.error().position->column, 0);
    EXPECT_FALSE(manifest.error().message.empty());
}

/**
 * A target entry missing its entry point is reported against that entry.
 */
TEST(ManifestParserTest, ReportsTargetEntryMissingSource)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "my-app"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin.src");
    EXPECT_EQ(manifest.error().message, "required key is missing");
}

/**
 * A [bin] table where an array of tables is expected is reported as such.
 */
TEST(ManifestParserTest, ReportsTargetDeclarationOfWrongShape)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[bin]
name = "my-app"
src = "src/main.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin");
    EXPECT_EQ(manifest.error().message, "must be an array of tables");
}

/**
 * loadManifest reads the file from disk.
 */
TEST(ManifestParserTest, LoadsManifestFromDisk)
{
    const TempDirectory temp;
    const auto file = temp.writeFile(ManifestName, "[package]\nname = \"disk-app\"\nversion = \"0.2.0\"\n");

    const auto manifest = loadManifest(file);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_EQ(manifest->package.name, "disk-app");
    EXPECT_EQ(manifest->package.version, "0.2.0");
}

/**
 * A manifest that cannot be opened is reported without inventing a position.
 */
TEST(ManifestParserTest, ReportsManifestThatCannotBeOpened)
{
    const TempDirectory temp;
    const auto missing = temp.path() / ManifestName;

    const auto manifest = loadManifest(missing);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_FALSE(manifest.error().position.has_value());
    EXPECT_TRUE(manifest.error().key.empty());
    EXPECT_EQ(manifest.error().message, "cannot open the manifest");
    EXPECT_EQ(manifest.error().file, missing);
}

/**
 * describe() renders the file, the position when known, and the key.
 */
TEST(ManifestParserTest, DescribeRendersWhatIsKnown)
{
    const ManifestError withEverything{.file = "scrap.toml",
                                       .position = SourcePosition{.line = 4, .column = 9},
                                       .key = "bin.name",
                                       .message = "must be a string"};
    EXPECT_EQ(describe(withEverything), "scrap.toml:4:9: bin.name: must be a string");

    const ManifestError withoutKey{.file = "scrap.toml",
                                   .position = SourcePosition{.line = 2, .column = 1},
                                   .key = {},
                                   .message = "unexpected token"};
    EXPECT_EQ(describe(withoutKey), "scrap.toml:2:1: unexpected token");

    const ManifestError fileOnly{
        .file = "a/scrap.toml", .position = std::nullopt, .key = {}, .message = "cannot open the manifest"};
    EXPECT_EQ(describe(fileOnly), "a/scrap.toml: cannot open the manifest");
}

/**
 * A mistyped table name is reported rather than ignored. Ignoring it would
 * turn a typo into silence, since a manifest missing its declarations is
 * indistinguishable from one that never had any.
 */
TEST(ManifestParserTest, RejectsUnknownTopLevelKey)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bins]]
name = "my-app"
src = "src/main.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bins");
    EXPECT_NE(manifest.error().message.find("unknown key"), std::string::npos);
    EXPECT_NE(manifest.error().message.find("bin"), std::string::npos);
}

/**
 * Declaring targets is recorded even when the declaration is empty, because
 * "this project builds nothing" is a different statement from declaring
 * nothing at all.
 */
TEST(ManifestParserTest, RecordsAnEmptyTargetDeclaration)
{
    // bin must precede the [package] header to be a top-level key: anything
    // written after it belongs to that table instead.
    constexpr std::string_view text = R"(
bin = []

[package]
name = "my-app"
version = "0.1.0"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->targets.empty());
    EXPECT_TRUE(manifest->declaresTargets);
}

/**
 * A declaration that does list targets is recorded the same way.
 */
TEST(ManifestParserTest, RecordsThatTargetsWereDeclared)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "my-app"
src = "src/main.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_TRUE(manifest.has_value());
    EXPECT_TRUE(manifest->declaresTargets);
}

/**
 * An absolute entry point is rejected: joining it onto the project root would
 * discard the root entirely.
 */
TEST(ManifestParserTest, RejectsAbsoluteEntryPoint)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "my-app"
src = "/etc/passwd"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin.src");
    EXPECT_EQ(manifest.error().message, "must be relative to the project root");
    EXPECT_TRUE(manifest.error().position.has_value());
}

/**
 * An entry point that climbs out of the project is rejected too.
 */
TEST(ManifestParserTest, RejectsEntryPointLeavingTheProject)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "my-app"
src = "../../outside.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin.src");
    EXPECT_EQ(manifest.error().message, "must not leave the project root");
}

/**
 * A target name reaches disk as the last component of an output path, so one
 * carrying a separator is rejected.
 */
TEST(ManifestParserTest, RejectsTargetNameWithPathSeparator)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "../../evil"
src = "src/main.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin.name");
    EXPECT_EQ(manifest.error().message, "must not contain a path separator");
}

/**
 * The package name is held to the same rule, since it becomes the artifact
 * name when no target is declared.
 */
TEST(ManifestParserTest, RejectsPackageNameWithPathSeparator)
{
    constexpr std::string_view text = "[package]\nname = \"../../evil\"\nversion = \"0.1.0\"\n";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package.name");
    EXPECT_EQ(manifest.error().message, "must not contain a path separator");
}

/**
 * Two targets of the same name would collide on one output path.
 */
TEST(ManifestParserTest, RejectsDuplicateTargetNames)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "x"
src = "src/a.cpp"

[[bin]]
name = "x"
src = "src/b.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin.name");
    EXPECT_EQ(manifest.error().message, "duplicate target name");
    ASSERT_TRUE(manifest.error().position.has_value());
    EXPECT_EQ(manifest.error().position->line, 11);
}

/**
 * The collision is caught across kinds as well, not only within one array.
 */
TEST(ManifestParserTest, RejectsDuplicateNameAcrossKinds)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "z"
src = "src/a.cpp"

[[lib]]
name = "z"
src = "src/b.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "lib.name");
    EXPECT_EQ(manifest.error().message, "duplicate target name");
}

/**
 * An empty manifest is a readable file, so it reaches the parser and is
 * reported for what it lacks rather than as a read failure.
 */
TEST(ManifestParserTest, LoadsAnEmptyManifestAsAMissingPackage)
{
    const TempDirectory temp;
    const auto file = temp.writeFile(ManifestName, "");

    const auto manifest = loadManifest(file);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package");
    EXPECT_EQ(manifest.error().message, "required table is missing");
}

/**
 * A directory named scrap.toml opens as a stream on some platforms and then
 * yields nothing, which would otherwise be indistinguishable from an empty
 * manifest and reported as a missing [package].
 */
TEST(ManifestParserTest, ReportsAManifestThatIsADirectory)
{
    const TempDirectory temp;
    const auto decoy = temp.makeDirectory(ManifestName);

    const auto manifest = loadManifest(decoy);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().message, "cannot open the manifest");
    EXPECT_FALSE(manifest.error().position.has_value());
}

/**
 * A declaration written below the [package] header lands inside that table
 * rather than at the top level, which is a mistake easy to make and
 * impossible to see in the result. It is reported where it was written.
 */
TEST(ManifestParserTest, RejectsATargetDeclarationNestedInPackage)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"
bin = []
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "package.bin");
    EXPECT_NE(manifest.error().message.find("unknown key"), std::string::npos);
}

/**
 * A mistyped key inside a target entry is reported too, rather than leaving
 * the entry looking like it is missing a required key it actually has.
 */
TEST(ManifestParserTest, RejectsUnknownKeyInATargetEntry)
{
    constexpr std::string_view text = R"(
[package]
name = "my-app"
version = "0.1.0"

[[bin]]
name = "my-app"
source = "src/main.cpp"
)";

    const auto manifest = parseManifest(text, ManifestName);

    ASSERT_FALSE(manifest.has_value());
    EXPECT_EQ(manifest.error().key, "bin.source");
    EXPECT_NE(manifest.error().message.find("unknown key"), std::string::npos);
}
