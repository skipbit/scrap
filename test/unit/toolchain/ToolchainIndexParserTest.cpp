#include "toolchain/ToolchainIndexParser.h"

#include "toolchain/ToolchainIndex.h"
#include "toolchain/ToolchainVersion.h"

#include <gtest/gtest.h>

#include <expected>
#include <string>
#include <string_view>
#include <variant>

using namespace scrap::Toolchain;

namespace {

constexpr std::string_view Header = R"(
[index]
schema_version = 1
generated_at = "2026-10-26T15:01:53Z"
generator = "scripts/generate-index.sh"
)";

constexpr std::string_view Digest = "sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
constexpr std::string_view Sha256 = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";

/**
 * @p text read by the scrap these tests stand for, 0.4.0.
 */
std::expected<ToolchainIndex, ToolchainIndexError> readIndex(std::string_view text)
{
    return parseToolchainIndex(text, ToolchainVersion::parse("0.4.0").value());
}

/**
 * An index holding @p toolchains below the usual header.
 */
std::string indexWith(std::string_view toolchains)
{
    return std::string{ Header } + std::string{ toolchains };
}

/**
 * One gcc toolchain whose ingot holds @p ingotKeys, which may break it.
 */
std::string gccWithIngot(std::string_view ingotKeys)
{
    return indexWith(R"(
[[toolchains]]
family = "gcc"
version = "15.2.0"
status = "active"
compiler = "g++"

[[toolchains.ingots]]
platform = "linux"
arch = "x86_64"
)" + std::string{ ingotKeys });
}

/**
 * Why @p text is refused, or an empty string when it is read.
 */
std::string faultOf(std::string_view text)
{
    const auto index = readIndex(text);
    return index.has_value() ? std::string{} : index.error().message;
}

}  // namespace

/**
 * Every key the index contract names is read into its field.
 */
TEST(ToolchainIndexParserTest, ReadsAToolchainAndItsIngots)
{
    const auto index = readIndex(indexWith(R"(
[[toolchains]]
family = "gcc"
version = "15.2.0"
status = "deprecated"
license = "GPL-3.0-or-later WITH GCC-exception-3.1"
source_type = "build"
compiler = "g++"
c_compiler = "gcc"
origin = "scrap-release"
min_scrap_version = "0.4.0"

[[toolchains.ingots]]
platform = "linux"
arch = "x86_64"
registry = "ghcr.io/skipbit/scrap-toolchain"
tag = "gcc:15.2.0"
digest = ")" + std::string{ Digest } + R"("
glibc_version = "2.17"
)"));

    ASSERT_TRUE(index.has_value()) << index.error().message;
    ASSERT_EQ(index->toolchains.size(), 1U);
    const IndexedToolchain& gcc = index->toolchains[0];
    EXPECT_EQ(gcc.family, "gcc");
    EXPECT_EQ(gcc.version.text(), "15.2.0");
    EXPECT_EQ(gcc.status, ToolchainStatus::Deprecated);
    EXPECT_EQ(gcc.compiler, "g++");
    EXPECT_EQ(gcc.cCompiler, "gcc");
    ASSERT_TRUE(gcc.minScrapVersion.has_value());
    EXPECT_EQ(gcc.minScrapVersion->text(), "0.4.0");
    ASSERT_EQ(gcc.ingots.size(), 1U);
    const Ingot& ingot = gcc.ingots[0];
    EXPECT_EQ(ingot.platform, "linux");
    EXPECT_EQ(ingot.arch, "x86_64");
    ASSERT_TRUE(ingot.glibcVersion.has_value());
    EXPECT_EQ(ingot.glibcVersion->text(), "2.17");
    const auto* source = std::get_if<RegistrySource>(&ingot.source);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->registry, "ghcr.io/skipbit/scrap-toolchain");
    EXPECT_EQ(source->tag, "gcc:15.2.0");
    EXPECT_EQ(source->digest, Digest);
}

/**
 * Each ingot is fetched by the keys it holds, whatever the toolchain's
 * source_type says, so one toolchain may hold both kinds.
 */
TEST(ToolchainIndexParserTest, TellsTheSourceOfEachIngotByItsKeys)
{
    const auto index = readIndex(indexWith(R"(
[[toolchains]]
family = "llvm"
version = "23.1.4"
status = "active"
source_type = "fetch"
compiler = "clang"

[[toolchains.ingots]]
platform = "linux"
arch = "x86_64"
registry = "ghcr.io/skipbit/scrap-toolchain"
tag = "llvm:23.1.4"
digest = ")" + std::string{ Digest } + R"("

[[toolchains.ingots]]
platform = "darwin"
arch = "aarch64"
url = "https://example.com/llvm.tar.xz"
sha256 = ")" + std::string{ Sha256 } + R"("
)"));

    ASSERT_TRUE(index.has_value()) << index.error().message;
    const auto& ingots = index->toolchains.at(0).ingots;
    ASSERT_EQ(ingots.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<RegistrySource>(ingots[0].source));
    const auto* url = std::get_if<UrlSource>(&ingots[1].source);
    ASSERT_NE(url, nullptr);
    EXPECT_EQ(url->url, "https://example.com/llvm.tar.xz");
    EXPECT_EQ(url->sha256, Sha256);
    EXPECT_FALSE(ingots[1].glibcVersion.has_value());
}

/**
 * Optional keys left out leave their fields empty, and a toolchain written
 * without ingots, as the index writes one with none, holds none.
 */
TEST(ToolchainIndexParserTest, LeavesOutWhatIsNotWritten)
{
    const auto index = readIndex(indexWith(R"(
[[toolchains]]
family = "gcc"
version = "16.1.0"
status = "disabled"
compiler = "g++"
)"));

    ASSERT_TRUE(index.has_value()) << index.error().message;
    const IndexedToolchain& gcc = index->toolchains.at(0);
    EXPECT_EQ(gcc.status, ToolchainStatus::Disabled);
    EXPECT_FALSE(gcc.cCompiler.has_value());
    EXPECT_FALSE(gcc.minScrapVersion.has_value());
    EXPECT_TRUE(gcc.ingots.empty());
}

/**
 * An index listing no toolchains holds none.
 */
TEST(ToolchainIndexParserTest, ReadsAnIndexWithoutToolchains)
{
    const auto index = readIndex(Header);

    ASSERT_TRUE(index.has_value()) << index.error().message;
    EXPECT_TRUE(index->toolchains.empty());
}

/**
 * Keys this version does not know are passed over at every level, so the
 * maker can add one without stopping older versions.
 */
TEST(ToolchainIndexParserTest, PassesOverUnknownKeys)
{
    std::string text = R"(
future_table = { a = 1 }

[index]
schema_version = 1
future_key = "x"

[[toolchains]]
family = "gcc"
version = "15.2.0"
status = "active"
compiler = "g++"
future_key = "x"

[[toolchains.ingots]]
platform = "freebsd"
arch = "riscv64"
url = "https://example.com/gcc.tar.xz"
sha256 = ")";
    text += Sha256;
    text += R"("
origin = "upstream"
)";

    const auto index = readIndex(text);

    ASSERT_TRUE(index.has_value()) << index.error().message;
    EXPECT_EQ(index->toolchains.at(0).ingots.at(0).platform, "freebsd");
}

/**
 * A format other than 1 is refused as one this version cannot read, and
 * the declared value is kept to report.
 */
TEST(ToolchainIndexParserTest, RefusesAnotherFormat)
{
    for (const auto& [declared, reported] : { std::pair<std::string_view, std::string_view>{ "2", "2" }, { "0", "0" }, { "\"1\"", "\"1\"" }, { "1.0", "a float" }, { "true", "a boolean" }, { "[1]", "an array" }, { "{}", "a table" }, { "2026-10-31", "a date or time" } }) {
        const auto index = readIndex("[index]\nschema_version = " + std::string{ declared } + "\n");
        ASSERT_FALSE(index.has_value()) << declared;
        EXPECT_EQ(index.error().kind, ToolchainIndexErrorKind::UnsupportedFormat) << declared;
        EXPECT_EQ(index.error().message, reported) << declared;
    }
}

/**
 * Text that is not TOML, or lacks the header that declares the format, is
 * malformed.
 */
TEST(ToolchainIndexParserTest, RefusesTextWithoutAFormat)
{
    const auto notToml = readIndex("\n[index");
    ASSERT_FALSE(notToml.has_value());
    EXPECT_EQ(notToml.error().kind, ToolchainIndexErrorKind::Malformed);
    EXPECT_TRUE(notToml.error().message.starts_with("line 2: ")) << notToml.error().message;
    EXPECT_EQ(faultOf("[[toolchains]]\nfamily = \"gcc\"\n"), "line 1: index: is missing");
    EXPECT_EQ(faultOf("index = 1\n"), "line 1: index: must be a table");
    EXPECT_EQ(faultOf("[index]\ngenerator = \"x\"\n"), "line 1: index.schema_version: is missing");
}

/**
 * A toolchain missing a required key, or holding a value of the wrong
 * shape, refuses the whole index.
 */
TEST(ToolchainIndexParserTest, RefusesAMalformedToolchain)
{
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n")), "line 6: toolchains[0].family: is missing");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.x\"\nstatus = \"active\"\ncompiler = \"g++\"\n")), "line 8: toolchains[0].version: must be numbers joined by dots");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n")), "line 7: toolchains[0].family: must not be empty");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = 1\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n")), "line 7: toolchains[0].family: must be a string");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\n")), "line 6: toolchains[0].compiler: is missing");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\nc_compiler = \"../gcc\"\n")), "line 11: toolchains[0].c_compiler: must hold only lowercase letters, digits, '-', '_', '.' and '+', and not be '.' or '..'");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\nc_compiler = 1\n")), "line 11: toolchains[0].c_compiler: must be a string");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\nmin_scrap_version = \"0.4.0-rc1\"\n")), "line 11: toolchains[0].min_scrap_version: must be numbers joined by dots");
    EXPECT_EQ(faultOf("toolchains = 1\n" + std::string{ Header }), "line 1: toolchains: must be an array of tables");
    EXPECT_EQ(faultOf("toolchains = [1]\n" + std::string{ Header }), "line 1: toolchains: must be an array of tables");
}

/**
 * A status this version does not know refuses the whole index, since it
 * may change whether the toolchain can be installed.
 */
TEST(ToolchainIndexParserTest, RefusesAnUnknownStatus)
{
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"preview\"\ncompiler = \"g++\"\n")), "line 9: toolchains[0].status: must be active, deprecated or disabled");
}

/**
 * The same family and version listed twice leaves no single entry to
 * choose, and refuses the whole index.
 */
TEST(ToolchainIndexParserTest, RefusesARepeatedToolchain)
{
    const std::string_view gcc = "[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n";

    EXPECT_EQ(faultOf(indexWith(std::string{ gcc } + std::string{ gcc })), "line 11: toolchains[1]: repeats gcc@15.2.0");
}

/**
 * An ingot holding both kinds of source, or neither, refuses the whole
 * index rather than leaving which to fetch to guesswork.
 */
TEST(ToolchainIndexParserTest, RefusesAnIngotWithBothSourcesOrNeither)
{
    EXPECT_EQ(faultOf(gccWithIngot("registry = \"r\"\ntag = \"t\"\ndigest = \"" + std::string{ Digest } + "\"\nurl = \"u\"\nsha256 = \"" + std::string{ Sha256 } + "\"\n")),
              "line 13: toolchains[0].ingots[0]: names both a registry and a url");
    EXPECT_EQ(faultOf(gccWithIngot("digest = \"" + std::string{ Digest } + "\"\nsha256 = \"" + std::string{ Sha256 } + "\"\n")),
              "line 13: toolchains[0].ingots[0]: names both a registry and a url");
    EXPECT_EQ(faultOf(gccWithIngot("")), "line 13: toolchains[0].ingots[0]: names neither a registry nor a url");
}

/**
 * A source missing one of its keys, or holding a checksum of the wrong
 * shape, refuses the whole index.
 */
TEST(ToolchainIndexParserTest, RefusesAnIncompleteSource)
{
    EXPECT_EQ(faultOf(gccWithIngot("registry = \"r\"\ndigest = \"" + std::string{ Digest } + "\"\n")), "line 13: toolchains[0].ingots[0].tag: is missing");
    EXPECT_EQ(faultOf(gccWithIngot("url = \"u\"\n")), "line 13: toolchains[0].ingots[0].sha256: is missing");
    EXPECT_EQ(faultOf(gccWithIngot("registry = \"r\"\ntag = \"t\"\ndigest = \"" + std::string{ Sha256 } + "\"\n")),
              "line 18: toolchains[0].ingots[0].digest: must be sha256: and 64 lowercase hex digits");
    EXPECT_EQ(faultOf(gccWithIngot("registry = \"r\"\ntag = \"t\"\ndigest = \"sha256:" + std::string{ Sha256.substr(1) } + "\"\n")),
              "line 18: toolchains[0].ingots[0].digest: must be sha256: and 64 lowercase hex digits");
    EXPECT_EQ(faultOf(gccWithIngot("url = \"u\"\nsha256 = \"" + std::string{ "FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210" } + "\"\n")),
              "line 17: toolchains[0].ingots[0].sha256: must be 64 lowercase hex digits");
}

/**
 * An ingot without its OS or architecture, with a glibc version of the
 * wrong shape, or repeating another for the same host refuses the whole
 * index.
 */
TEST(ToolchainIndexParserTest, RefusesAMalformedIngot)
{
    const std::string url = "url = \"u\"\nsha256 = \"" + std::string{ Sha256 } + "\"\n";

    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n[[toolchains.ingots]]\narch = \"x86_64\"\n" + url)),
              "line 11: toolchains[0].ingots[0].platform: is missing");
    EXPECT_EQ(faultOf(gccWithIngot(url + "glibc_version = \"2.x\"\n")), "line 18: toolchains[0].ingots[0].glibc_version: must be numbers joined by dots");
    std::string twice = url;
    twice += "[[toolchains.ingots]]\nplatform = \"linux\"\narch = \"x86_64\"\n";
    twice += url;
    EXPECT_EQ(faultOf(gccWithIngot(twice)),
              "line 18: toolchains[0].ingots[1]: repeats an ingot for linux-x86_64");
}

/**
 * A family, compiler, platform or arch that could lead out of the toolchain
 * store refuses the whole index.
 */
TEST(ToolchainIndexParserTest, RefusesANameThatCannotStandInAPath)
{
    const std::string rule = "must hold only lowercase letters, digits, '-', '_', '.' and '+', and not be '.' or '..'";
    const std::string url = "url = \"u\"\nsha256 = \"" + std::string{ Sha256 } + "\"\n";

    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"../gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n")), "line 7: toolchains[0].family: " + rule);
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"../../bin/sh\"\n")), "line 10: toolchains[0].compiler: " + rule);
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n[[toolchains.ingots]]\nplatform = \"linux/x\"\narch = \"x86_64\"\n" + url)),
              "line 12: toolchains[0].ingots[0].platform: " + rule);
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"15.2.0\"\nstatus = \"active\"\ncompiler = \"g++\"\n[[toolchains.ingots]]\nplatform = \"linux\"\narch = \"..\"\n" + url)),
              "line 13: toolchains[0].ingots[0].arch: " + rule);
}

/**
 * A toolchain for a newer scrap is kept by its family, version and the
 * scrap it needs, unread past them and disabled, so a shape only that scrap
 * knows leaves the rest of the index usable.
 */
TEST(ToolchainIndexParserTest, KeepsAToolchainForANewerScrapUnread)
{
    const auto index = readIndex(indexWith(R"(
[[toolchains]]
family = "gcc"
version = "16.1.0"
min_scrap_version = "0.5.0"
status = "preview"

[[toolchains.ingots]]
platform = "linux"
arch = "x86_64"
layout = "oci"

[[toolchains]]
family = "gcc"
version = "15.2.0"
status = "active"
compiler = "g++"
)"));

    ASSERT_TRUE(index.has_value()) << index.error().message;
    ASSERT_EQ(index->toolchains.size(), 2U);
    const IndexedToolchain& newer = index->toolchains.at(0);
    EXPECT_EQ(newer.family, "gcc");
    EXPECT_EQ(newer.version.text(), "16.1.0");
    EXPECT_EQ(newer.minScrapVersion.value().text(), "0.5.0");
    EXPECT_EQ(newer.status, ToolchainStatus::Disabled);
    EXPECT_TRUE(newer.compiler.empty());
    EXPECT_TRUE(newer.ingots.empty());
    EXPECT_EQ(index->toolchains.at(1).status, ToolchainStatus::Active);
}

/**
 * A toolchain for this scrap or an older one is read in full, and one for a
 * newer scrap still needs a family and version this scrap can read.
 */
TEST(ToolchainIndexParserTest, ReadsInFullAToolchainThisScrapMeets)
{
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"gcc\"\nversion = \"16.1.0\"\nmin_scrap_version = \"0.4.0\"\nstatus = \"preview\"\n")),
              "line 10: toolchains[0].status: must be active, deprecated or disabled");
    EXPECT_EQ(faultOf(indexWith("[[toolchains]]\nfamily = \"../gcc\"\nversion = \"16.1.0\"\nmin_scrap_version = \"0.5.0\"\n")),
              "line 7: toolchains[0].family: must hold only lowercase letters, digits, '-', '_', '.' and '+', and not be '.' or '..'");
}

/**
 * A string holding a control character refuses the whole index, so no
 * value read from it can break a line it is later written into.
 */
TEST(ToolchainIndexParserTest, RefusesAControlCharacter)
{
    const std::string rule = "must not contain a control character";

    EXPECT_EQ(faultOf(gccWithIngot("url = \"https://example.com/a\\n\"\nsha256 = \"" + std::string{ Sha256 } + "\"\n")), "line 16: toolchains[0].ingots[0].url: " + rule);
    EXPECT_EQ(faultOf(gccWithIngot("registry = \"r\\t\"\ntag = \"t\"\ndigest = \"" + std::string{ Digest } + "\"\n")), "line 16: toolchains[0].ingots[0].registry: " + rule);
    EXPECT_EQ(faultOf(gccWithIngot("registry = \"r\"\ntag = \"gcc:15\\r\"\ndigest = \"" + std::string{ Digest } + "\"\n")), "line 17: toolchains[0].ingots[0].tag: " + rule);
}
