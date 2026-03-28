#include <gtest/gtest.h>

#include "command/DefaultHelpRenderer.h"
#include "command/DefaultVersionRenderer.h"

using namespace scrap::Command;

namespace {

auto makeHelpEntry(const std::string& name,
                   CommandSource source,
                   const std::string& category = "",
                   const std::string& description = "") -> HelpEntry
{
    HelpEntry entry;
    entry.spec.name = name;
    entry.spec.description = description;
    entry.spec.category = category;
    entry.source = source;
    return entry;
}

}  // namespace

// --- DefaultHelpRenderer::renderGlobal ---

TEST(DefaultHelpRendererTest, RenderGlobal_GroupsByCategory)
{
    DefaultHelpRenderer renderer;
    std::vector<HelpEntry> entries;
    entries.push_back(makeHelpEntry("build", CommandSource::Builtin, "Build Commands", "Build the project"));
    entries.push_back(makeHelpEntry("run", CommandSource::Builtin, "Build Commands", "Run the project"));
    entries.push_back(makeHelpEntry("new", CommandSource::Builtin, "Project Commands", "Create a new project"));

    auto result = renderer.renderGlobal(entries);

    EXPECT_NE(result.find("Build Commands:"), std::string::npos);
    EXPECT_NE(result.find("Project Commands:"), std::string::npos);

    // build and run should appear under the same category header
    auto buildCommandsPos = result.find("Build Commands:");
    auto projectCommandsPos = result.find("Project Commands:");
    auto buildPos = result.find("build");
    auto runPos = result.find("run");
    auto newPos = result.find("new");

    EXPECT_GT(buildPos, buildCommandsPos);
    EXPECT_GT(runPos, buildCommandsPos);
    EXPECT_LT(buildPos, projectCommandsPos);
    EXPECT_LT(runPos, projectCommandsPos);
    EXPECT_GT(newPos, projectCommandsPos);
}

TEST(DefaultHelpRendererTest, RenderGlobal_AlignsColumns)
{
    DefaultHelpRenderer renderer;
    std::vector<HelpEntry> entries;
    entries.push_back(makeHelpEntry("build", CommandSource::Builtin, "", "Build the project"));
    entries.push_back(makeHelpEntry("b", CommandSource::Builtin, "", "Short name command"));

    auto result = renderer.renderGlobal(entries);

    // Extract individual lines for comparison.
    // The output should have aligned descriptions: both "Build the project" and
    // "Short name command" start at the same column despite different name lengths.
    auto extractLine = [](const std::string& text, const std::string& linePrefix) -> std::string {
        auto pos = text.find(linePrefix);
        if (pos == std::string::npos)
            return {};
        auto end = text.find('\n', pos);
        return text.substr(pos, end - pos);
    };

    auto buildLine = extractLine(result, "    build");
    auto bLine = extractLine(result, "    b ");

    ASSERT_FALSE(buildLine.empty());
    ASSERT_FALSE(bLine.empty());

    auto buildDescPos = buildLine.find("Build the project");
    auto bDescPos = bLine.find("Short name command");
    EXPECT_NE(buildDescPos, std::string::npos);
    EXPECT_NE(bDescPos, std::string::npos);
    EXPECT_EQ(buildDescPos, bDescPos);
}

TEST(DefaultHelpRendererTest, RenderGlobal_EmptyEntries)
{
    DefaultHelpRenderer renderer;
    std::vector<HelpEntry> entries;

    auto result = renderer.renderGlobal(entries);

    EXPECT_NE(result.find("USAGE: scrap [OPTIONS] <COMMAND>"), std::string::npos);
    EXPECT_NE(result.find("See 'scrap help <command>' for more information."), std::string::npos);
}

TEST(DefaultHelpRendererTest, RenderGlobal_SeparatesBuiltinExternalProject)
{
    DefaultHelpRenderer renderer;
    std::vector<HelpEntry> entries;
    entries.push_back(makeHelpEntry("build", CommandSource::Builtin, "Build Commands", "Build the project"));
    entries.push_back(makeHelpEntry("lint", CommandSource::External, "", "Run linter"));
    entries.push_back(makeHelpEntry("deploy", CommandSource::Project, "", "Deploy project"));

    auto result = renderer.renderGlobal(entries);

    auto builtinPos = result.find("Build Commands:");
    auto externalPos = result.find("External Commands:");
    auto projectPos = result.find("Project Commands:");

    EXPECT_NE(builtinPos, std::string::npos);
    EXPECT_NE(externalPos, std::string::npos);
    EXPECT_NE(projectPos, std::string::npos);

    // Builtin appears first, then External, then Project
    EXPECT_LT(builtinPos, externalPos);
    EXPECT_LT(externalPos, projectPos);
}

// --- DefaultHelpRenderer::renderCommand ---

TEST(DefaultHelpRendererTest, RenderCommand_WithSubcommands)
{
    DefaultHelpRenderer renderer;
    CommandSpec spec;
    spec.name = "toolchain";
    spec.description = "Manage toolchains";

    CommandSpec sub1;
    sub1.name = "install";
    sub1.description = "Install a toolchain";

    CommandSpec sub2;
    sub2.name = "list";
    sub2.description = "List available toolchains";

    spec.subcommands = {sub1, sub2};

    auto result = renderer.renderCommand(spec);

    EXPECT_NE(result.find("USAGE: scrap toolchain"), std::string::npos);
    EXPECT_NE(result.find("<COMMAND>"), std::string::npos);
    EXPECT_NE(result.find("SUBCOMMANDS:"), std::string::npos);
    EXPECT_NE(result.find("install"), std::string::npos);
    EXPECT_NE(result.find("Install a toolchain"), std::string::npos);
    EXPECT_NE(result.find("list"), std::string::npos);
    EXPECT_NE(result.find("List available toolchains"), std::string::npos);
}

TEST(DefaultHelpRendererTest, RenderCommand_WithOptions)
{
    DefaultHelpRenderer renderer;
    CommandSpec spec;
    spec.name = "build";
    spec.description = "Build the project";

    OptionDef opt;
    opt.longName = "release";
    opt.shortName = 'r';
    opt.type = OptionValueType::Bool;
    opt.description = "Build in release mode";
    spec.options.named.push_back(opt);

    OptionDef opt2;
    opt2.longName = "jobs";
    opt2.shortName = 'j';
    opt2.type = OptionValueType::Int64;
    opt2.description = "Number of parallel jobs";
    spec.options.named.push_back(opt2);

    auto result = renderer.renderCommand(spec);

    EXPECT_NE(result.find("USAGE: scrap build [OPTIONS]"), std::string::npos);
    EXPECT_NE(result.find("OPTIONS:"), std::string::npos);
    EXPECT_NE(result.find("-r, --release"), std::string::npos);
    EXPECT_NE(result.find("Build in release mode"), std::string::npos);
    EXPECT_NE(result.find("-j, --jobs"), std::string::npos);
    EXPECT_NE(result.find("Number of parallel jobs"), std::string::npos);
}

TEST(DefaultHelpRendererTest, RenderCommand_WithPositionalsInUsage)
{
    DefaultHelpRenderer renderer;
    CommandSpec spec;
    spec.name = "new";
    spec.description = "Create a new project";

    PositionalDef pos;
    pos.name = "name";
    pos.description = "Project name";
    pos.required = true;
    spec.options.positional.push_back(pos);

    PositionalDef optionalPos;
    optionalPos.name = "path";
    optionalPos.description = "Output path";
    optionalPos.required = false;
    spec.options.positional.push_back(optionalPos);

    auto result = renderer.renderCommand(spec);

    EXPECT_NE(result.find("USAGE: scrap new <name> [path]"), std::string::npos);
}

TEST(DefaultHelpRendererTest, RenderCommand_NoOptionsOmitsSection)
{
    DefaultHelpRenderer renderer;
    CommandSpec spec;
    spec.name = "clean";
    spec.description = "Clean build artifacts";

    auto result = renderer.renderCommand(spec);

    EXPECT_NE(result.find("USAGE: scrap clean"), std::string::npos);
    EXPECT_EQ(result.find("[OPTIONS]"), std::string::npos);
    EXPECT_EQ(result.find("OPTIONS:"), std::string::npos);
}

// --- DefaultVersionRenderer ---

TEST(DefaultVersionRendererTest, ReturnsVersionString)
{
    DefaultVersionRenderer renderer;
    auto result = renderer.render();

    // scrap::version() returns "scrap <version>", so it should not be empty
    // and should contain the application name
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("scrap"), std::string::npos);
}
