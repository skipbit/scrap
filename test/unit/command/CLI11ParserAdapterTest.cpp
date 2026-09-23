#include "command/driver/CLI11ParserAdapter.h"

#include <gtest/gtest.h>

using namespace scrap::Command;

namespace {

/**
 * Helper: build a minimal CommandSpec with no options.
 */
CommandSpec makeSpec(const std::string& name, const std::string& desc = "")
{
    CommandSpec spec;
    spec.name = name;
    spec.description = desc;
    return spec;
}

/**
 * Helper: build a CommandSpec with child subcommands.
 */
CommandSpec makeSpecWithSubs(const std::string& name, std::vector<CommandSpec> subs, const std::string& desc = "")
{
    auto spec = makeSpec(name, desc);
    spec.subcommands = std::move(subs);
    return spec;
}

/**
 * Helper: simulate argv from an initializer list.
 *
 * Stores C-strings in a vector whose lifetime is tied to the
 * returned object.  Use the .data() / .size() members to get
 * a span.
 */
class ArgvBuilder {
public:
    ArgvBuilder(std::initializer_list<const char*> args)
        : _args(args)
    {
    }

    [[nodiscard]] const char* const* data() const
    {
        return _args.data();
    }
    [[nodiscard]] std::size_t size() const
    {
        return _args.size();
    }
    [[nodiscard]] std::span<const char* const> span() const
    {
        return { _args.data(), _args.size() };
    }

private:
    std::vector<const char*> _args;
};

}  // namespace

// =============================================================================
// Simple command parsing
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_SimpleCommand)
{
    CLI11ParserAdapter adapter;

    std::vector<CommandSpec> specs = { makeSpec("build", "Build the project") };
    adapter.configure(specs);

    ArgvBuilder argv{ "scrap", "build" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, "build");
}

TEST(CLI11ParserAdapterTest, Parse_SubcommandPath)
{
    CLI11ParserAdapter adapter;

    std::vector<CommandSpec> specs = { makeSpecWithSubs("toolchain", { makeSpec("install", "Install"), makeSpec("list", "List") }) };
    adapter.configure(specs);

    ArgvBuilder argv{ "scrap", "toolchain", "install" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, "toolchain.install");
}

// =============================================================================
// Named options
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_BoolFlag)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "verbose",
        .shortName = 'v',
        .type = OptionValueType::Bool,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "build", "--verbose" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, "build");
    auto it = result->options.named.find("verbose");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_TRUE(std::get<bool>(it->second));
}

TEST(CLI11ParserAdapterTest, Parse_BoolFlag_ShortForm)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "verbose",
        .shortName = 'v',
        .type = OptionValueType::Bool,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "build", "-v" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("verbose");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_TRUE(std::get<bool>(it->second));
}

TEST(CLI11ParserAdapterTest, Parse_StringOption)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("new");
    spec.options.named.push_back(OptionDef{
        .longName = "template",
        .shortName = 't',
        .type = OptionValueType::String,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "new", "--template", "library" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("template");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_EQ(std::get<std::string>(it->second), "library");
}

TEST(CLI11ParserAdapterTest, Parse_Int64Option)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "jobs",
        .shortName = 'j',
        .type = OptionValueType::Int64,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "build", "--jobs", "8" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("jobs");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_EQ(std::get<std::int64_t>(it->second), 8);
}

// =============================================================================
// Positional arguments
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_Positional)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("new");
    spec.options.positional.push_back(PositionalDef{
        .name = "project-name",
        .description = "Name of the new project",
        .required = true,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "new", "myproject" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->options.positional.size(), 1);
    EXPECT_EQ(result->options.positional[0], "myproject");
}

TEST(CLI11ParserAdapterTest, Parse_MultiplePositionals)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("new");
    spec.options.positional.push_back(PositionalDef{
        .name = "project-name",
        .description = "Name of the new project",
        .required = true,
    });
    spec.options.positional.push_back(PositionalDef{
        .name = "directory",
        .description = "Target directory",
        .required = false,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "new", "myproject", "/tmp/dest" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->options.positional.size(), 2);
    EXPECT_EQ(result->options.positional[0], "myproject");
    EXPECT_EQ(result->options.positional[1], "/tmp/dest");
}

TEST(CLI11ParserAdapterTest, Parse_OmittedOptionalPositionalIsAbsent)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("help");
    spec.options.positional.push_back(PositionalDef{
        .name = "command",
        .description = "Command to get help for",
        .required = false,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "help" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->options.positional.empty());
}

TEST(CLI11ParserAdapterTest, Parse_OmittedTrailingPositionalKeepsTheGivenOnes)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("new");
    spec.options.positional.push_back(PositionalDef{
        .name = "project-name",
        .description = "Name of the new project",
        .required = true,
    });
    spec.options.positional.push_back(PositionalDef{
        .name = "directory",
        .description = "Target directory",
        .required = false,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "new", "myproject" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->options.positional.size(), 1);
    EXPECT_EQ(result->options.positional[0], "myproject");
}

TEST(CLI11ParserAdapterTest, Parse_ExplicitEmptyPositionalIsKept)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.positional.push_back(PositionalDef{
        .name = "path",
        .description = "Directory inside the project",
        .required = false,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "build", "" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->options.positional.size(), 1);
    EXPECT_EQ(result->options.positional[0], "");
}

// =============================================================================
// No subcommand (error)
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_NoSubcommand)
{
    CLI11ParserAdapter adapter;

    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ParseFailure>(result.error()));
}

// =============================================================================
// Nested subcommand help
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_NestedSubcommandHelp)
{
    CLI11ParserAdapter adapter;

    std::vector<CommandSpec> specs = { makeSpecWithSubs("toolchain", { makeSpec("install", "Install"), makeSpec("list", "List") }) };
    adapter.configure(specs);

    ArgvBuilder argv{ "scrap", "toolchain", "install", "--help" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    auto& directive = std::get<ParseDirective>(result.error());
    EXPECT_EQ(directive.kind, ParseDirectiveKind::HelpRequested);
    ASSERT_TRUE(directive.target.has_value());
    EXPECT_EQ(*directive.target, "toolchain.install");
}

// =============================================================================
// Help / Version directives
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_GlobalHelp)
{
    CLI11ParserAdapter adapter;

    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap", "--help" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    auto& interruption = result.error();
    ASSERT_TRUE(std::holds_alternative<ParseDirective>(interruption));

    auto& directive = std::get<ParseDirective>(interruption);
    EXPECT_EQ(directive.kind, ParseDirectiveKind::HelpRequested);
    EXPECT_FALSE(directive.target.has_value());
}

TEST(CLI11ParserAdapterTest, Parse_CommandHelp)
{
    CLI11ParserAdapter adapter;

    adapter.configure(std::vector{ makeSpec("build", "Build the project") });

    ArgvBuilder argv{ "scrap", "build", "--help" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    auto& directive = std::get<ParseDirective>(result.error());
    EXPECT_EQ(directive.kind, ParseDirectiveKind::HelpRequested);
    ASSERT_TRUE(directive.target.has_value());
    EXPECT_EQ(*directive.target, "build");
}

TEST(CLI11ParserAdapterTest, Parse_VersionFlag)
{
    CLI11ParserAdapter adapter;

    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap", "--version" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    auto& directive = std::get<ParseDirective>(result.error());
    EXPECT_EQ(directive.kind, ParseDirectiveKind::VersionRequested);
    EXPECT_FALSE(directive.target.has_value());
}

TEST(CLI11ParserAdapterTest, Parse_VersionShortFlag)
{
    CLI11ParserAdapter adapter;

    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap", "-V" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    auto& directive = std::get<ParseDirective>(result.error());
    EXPECT_EQ(directive.kind, ParseDirectiveKind::VersionRequested);
}

// =============================================================================
// Error cases
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_UnknownCommand)
{
    CLI11ParserAdapter adapter;

    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap", "nonexistent" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ParseFailure>(result.error()));
}

TEST(CLI11ParserAdapterTest, Parse_ChoiceValidation)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "profile",
        .shortName = std::nullopt,
        .type = OptionValueType::String,
        .required = false,
        .description = "Build profile",
        .defaultValue = std::nullopt,
        .choices = { "debug", "release" },
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "build", "--profile", "invalid" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ParseFailure>(result.error()));
}

// =============================================================================
// Default values
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_DefaultValue)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "profile",
        .shortName = std::nullopt,
        .type = OptionValueType::String,
        .required = false,
        .description = "Build profile",
        .defaultValue = OptionValue{ std::string{ "debug" } },
        .choices = {},
    });

    adapter.configure(std::vector{ spec });

    // Parse without providing --profile; the default should apply.
    ArgvBuilder argv{ "scrap", "build" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("profile");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_EQ(std::get<std::string>(it->second), "debug");
}
