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

/**
 * Helper: a command like run, with an optional path and arguments after "--".
 */
CommandSpec makeRunSpec()
{
    CommandSpec spec = makeSpec("run");
    spec.options.positional.push_back(PositionalDef{
        .name = "path",
        .description = "Directory inside the project",
        .required = false,
    });
    spec.options.trailing = TrailingDef{ .name = "args", .description = "Arguments for the program" };
    return spec;
}

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
    EXPECT_EQ(result->commandPath, (std::vector<std::string>{ "build" }));
}

TEST(CLI11ParserAdapterTest, Parse_SubcommandPath)
{
    CLI11ParserAdapter adapter;

    std::vector<CommandSpec> specs = { makeSpecWithSubs("toolchain", { makeSpec("install", "Install"), makeSpec("list", "List") }) };
    adapter.configure(specs);

    ArgvBuilder argv{ "scrap", "toolchain", "install" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, (std::vector<std::string>{ "toolchain", "install" }));
}

/**
 * The options of a nested command are read from the nested command.
 */
TEST(CLI11ParserAdapterTest, Parse_NestedCommandOption)
{
    CLI11ParserAdapter adapter;

    CommandSpec install = makeSpec("install", "Install");
    install.options.named.push_back(OptionDef{
        .longName = "force",
        .shortName = std::nullopt,
        .type = OptionValueType::Bool,
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
    });
    adapter.configure(std::vector{ makeSpecWithSubs("toolchain", { install }) });

    ArgvBuilder argv{ "scrap", "toolchain", "install", "--force" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("force");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_TRUE(std::get<bool>(it->second));
}

/**
 * A command whose name holds a dot keeps its options apart from a nested
 * command its name spells out.
 */
TEST(CLI11ParserAdapterTest, Parse_NameWithADotKeepsItsOptions)
{
    CLI11ParserAdapter adapter;

    CommandSpec dotted = makeSpec("toolchain.list");
    dotted.options.named.push_back(OptionDef{
        .longName = "verbose",
        .shortName = std::nullopt,
        .type = OptionValueType::Bool,
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
    });
    adapter.configure(std::vector{ dotted, makeSpecWithSubs("toolchain", { makeSpec("list", "List") }) });

    ArgvBuilder argv{ "scrap", "toolchain.list", "--verbose" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, (std::vector<std::string>{ "toolchain.list" }));
    auto it = result->options.named.find("verbose");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_TRUE(std::get<bool>(it->second));
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
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "build", "--verbose" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, (std::vector<std::string>{ "build" }));
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
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
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
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "new", "--template", "library" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("template");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_EQ(std::get<std::string>(it->second), "library");
}

/**
 * An empty value given on the command line is kept, so a command can tell it
 * from an option left out.
 */
TEST(CLI11ParserAdapterTest, Parse_ExplicitEmptyStringOptionIsKept)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "target",
        .shortName = std::nullopt,
        .type = OptionValueType::String,
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder given{ "scrap", "build", "--target", "" };
    auto result = adapter.parse(given.span());

    ASSERT_TRUE(result.has_value());
    auto it = result->options.named.find("target");
    ASSERT_NE(it, result->options.named.end());
    EXPECT_EQ(std::get<std::string>(it->second), "");

    ArgvBuilder omitted{ "scrap", "build" };
    auto without = adapter.parse(omitted.span());

    ASSERT_TRUE(without.has_value());
    EXPECT_EQ(without->options.named.count("target"), 0);
}

TEST(CLI11ParserAdapterTest, Parse_Int64Option)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("build");
    spec.options.named.push_back(OptionDef{
        .longName = "jobs",
        .shortName = 'j',
        .type = OptionValueType::Int64,
        .required = false,
        .description = "",
        .defaultValue = std::nullopt,
        .choices = {},
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

TEST(CLI11ParserAdapterTest, Parse_RepeatingPositionalTakesEveryWordLeft)
{
    CLI11ParserAdapter adapter;

    CommandSpec spec = makeSpec("pick");
    spec.options.positional.push_back(PositionalDef{
        .name = "first",
        .description = "First word",
        .required = true,
    });
    spec.options.positional.push_back(PositionalDef{
        .name = "rest",
        .description = "Other words",
        .required = false,
        .repeats = true,
    });

    adapter.configure(std::vector{ spec });

    ArgvBuilder argv{ "scrap", "pick", "alpha", "beta", "", "gamma" };
    auto result = adapter.parse(argv.span());
    ArgvBuilder omitted{ "scrap", "pick", "alpha" };
    auto without = adapter.parse(omitted.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->options.positional, (std::vector<std::string>{ "alpha", "beta", "", "gamma" }));
    ASSERT_TRUE(without.has_value());
    EXPECT_EQ(without->options.positional, std::vector<std::string>{ "alpha" });
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

/**
 * A command given a default of the wrong type is a fault in the commands,
 * not in the command line.
 */
TEST(CLI11ParserAdapterTest, Parse_MisconfiguredOptionIsAFault)
{
    CLI11ParserAdapter adapter;

    CommandSpec build = makeSpec("build");
    build.options.named.push_back(OptionDef{
        .longName = "jobs",
        .shortName = std::nullopt,
        .type = OptionValueType::Int64,
        .required = false,
        .description = "",
        .defaultValue = OptionValue{ std::string{ "four" } },
        .choices = {},
    });
    adapter.configure(std::vector{ build });

    ArgvBuilder argv{ "scrap", "build" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<ParserFault>(result.error()));
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
    EXPECT_EQ(directive.target, (std::vector<std::string>{ "toolchain", "install" }));
}

TEST(CLI11ParserAdapterTest, Parse_HelpKeepsTheWordsLeftOver)
{
    CLI11ParserAdapter adapter;

    std::vector<CommandSpec> specs = { makeSpecWithSubs("toolchain", { makeSpec("list", "List") }) };
    adapter.configure(specs);

    const auto targetOf = [&adapter](ArgvBuilder argv) {
        auto result = adapter.parse(argv.span());
        EXPECT_FALSE(result.has_value());
        return result.has_value() ? std::vector<std::string>{} : std::get<ParseDirective>(result.error()).target;
    };

    EXPECT_EQ(targetOf({ "scrap", "toolchain", "nosuch", "--help" }), (std::vector<std::string>{ "toolchain", "nosuch" }));
    EXPECT_EQ(targetOf({ "scrap", "toolchain", "--help", "a", "b" }), (std::vector<std::string>{ "toolchain", "a", "b" }));
    EXPECT_EQ(targetOf({ "scrap", "nosuch", "--help" }), std::vector<std::string>{ "nosuch" });
    EXPECT_EQ(targetOf({ "scrap", "toolchain", "nosuch", "list", "--help" }), (std::vector<std::string>{ "toolchain", "nosuch", "list" }));
    EXPECT_EQ(targetOf({ "scrap", "toolchain", "--bogus", "--help" }), std::vector<std::string>{ "toolchain" });
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
    EXPECT_TRUE(directive.target.empty());
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
    EXPECT_EQ(directive.target, std::vector<std::string>{ "build" });
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
    EXPECT_TRUE(directive.target.empty());
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

// =============================================================================
// Arguments after "--"
// =============================================================================

TEST(CLI11ParserAdapterTest, Parse_ArgumentsAfterTheSeparatorAreKeptAsWritten)
{
    CLI11ParserAdapter adapter;
    adapter.configure(std::vector{ makeRunSpec() });

    ArgvBuilder argv{ "scrap", "run", "--", "alpha", "--help", "", "-x", "--", "beta" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->options.positional.empty());
    EXPECT_EQ(result->options.trailing, (std::vector<std::string>{ "alpha", "--help", "", "-x", "--", "beta" }));
}

TEST(CLI11ParserAdapterTest, Parse_PositionalBeforeTheSeparator)
{
    CLI11ParserAdapter adapter;
    adapter.configure(std::vector{ makeRunSpec() });

    ArgvBuilder argv{ "scrap", "run", "app", "--", "alpha" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->options.positional, (std::vector<std::string>{ "app" }));
    EXPECT_EQ(result->options.trailing, (std::vector<std::string>{ "alpha" }));
}

TEST(CLI11ParserAdapterTest, Parse_NothingAfterTheSeparator)
{
    CLI11ParserAdapter adapter;
    adapter.configure(std::vector{ makeRunSpec() });

    ArgvBuilder argv{ "scrap", "run", "--" };
    auto result = adapter.parse(argv.span());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->options.trailing.empty());
}

TEST(CLI11ParserAdapterTest, Parse_ArgumentsAfterTheSeparatorForACommandThatTakesNone)
{
    CLI11ParserAdapter adapter;
    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap", "build", "--", "alpha", "beta" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ParseFailure>(result.error()));
    EXPECT_EQ(std::get<ParseFailure>(result.error()).message, "The following arguments were not expected: alpha beta");
}

TEST(CLI11ParserAdapterTest, Parse_OneArgumentAfterTheSeparatorForACommandThatTakesNone)
{
    CLI11ParserAdapter adapter;
    adapter.configure(std::vector{ makeSpec("build") });

    ArgvBuilder argv{ "scrap", "build", "--", "alpha" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ParseFailure>(result.error()));
    EXPECT_EQ(std::get<ParseFailure>(result.error()).message, "The following argument was not expected: alpha");
}

TEST(CLI11ParserAdapterTest, Parse_HelpBeforeTheSeparatorIsStillHelp)
{
    CLI11ParserAdapter adapter;
    adapter.configure(std::vector{ makeRunSpec() });

    ArgvBuilder argv{ "scrap", "run", "--help", "--", "alpha" };
    auto result = adapter.parse(argv.span());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<ParseDirective>(result.error()));
    EXPECT_EQ(std::get<ParseDirective>(result.error()).target, std::vector<std::string>{ "run" });
}

namespace {

/**
 * Helper: names the parser cannot hold, each failing a different rule.
 */
std::vector<std::string> refusedNames()
{
    return { "a:b", "a=b", "a b", "a{b", std::string{ "a" } + '\x01' + "b", "!x", "-x", std::string{ '\x01' } + "b", "\tb" };
}

/**
 * Helper: names the parser holds, including ones close to the refused ones.
 */
std::vector<std::string> acceptedNames()
{
    return { "build", "foo.sh", "a}b", "a\tb", std::string{ "a" } + '\x7f' + "b", std::string{ "a" } + '\x9b' + "b", "a\"b" };
}

}  // namespace

TEST(CLI11ParserAdapterTest, AcceptsName_NamesItCanHold)
{
    const CLI11ParserAdapter adapter;
    for (const auto& name : acceptedNames()) {
        EXPECT_TRUE(adapter.acceptsName(name)) << name;
    }
}

TEST(CLI11ParserAdapterTest, AcceptsName_RefusesNamesItCannotHold)
{
    const CLI11ParserAdapter adapter;
    for (const auto& name : refusedNames()) {
        EXPECT_FALSE(adapter.acceptsName(name)) << name;
    }
}

TEST(CLI11ParserAdapterTest, AcceptsName_RefusesAnEmptyName)
{
    const CLI11ParserAdapter adapter;
    EXPECT_FALSE(adapter.acceptsName(""));
}

TEST(CLI11ParserAdapterTest, AcceptsName_AgreesWithWhatParseCanHold)
{
    auto names = acceptedNames();
    const auto refused = refusedNames();
    names.insert(names.end(), refused.begin(), refused.end());
    for (const auto& name : names) {
        CLI11ParserAdapter adapter;
        adapter.configure(std::vector{ makeSpec(name), makeSpec("version") });

        ArgvBuilder argv{ "scrap", "version" };
        EXPECT_EQ(adapter.parse(argv.span()).has_value(), adapter.acceptsName(name)) << name;
    }
}
