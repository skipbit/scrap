#include <gtest/gtest.h>

#include "command/CommandEntry.h"
#include "command/CommandSpec.h"
#include "command/InvocationContext.h"
#include "command/OptionSchema.h"
#include "command/ParseResult.h"
#include "command/ParsedOptions.h"
#include "command/RuntimeEnvironment.h"

using namespace scrap::Command;

TEST(OptionValueTest, HoldsBool)
{
    OptionValue value = true;
    EXPECT_TRUE(std::holds_alternative<bool>(value));
    EXPECT_TRUE(std::get<bool>(value));
}

TEST(OptionValueTest, HoldsInt64)
{
    OptionValue value = std::int64_t{42};
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(value));
    EXPECT_EQ(std::get<std::int64_t>(value), 42);
}

TEST(OptionValueTest, HoldsString)
{
    OptionValue value = std::string{"hello"};
    EXPECT_TRUE(std::holds_alternative<std::string>(value));
    EXPECT_EQ(std::get<std::string>(value), "hello");
}

TEST(OptionValueTest, HoldsStringList)
{
    OptionValue value = std::vector<std::string>{"a", "b", "c"};
    EXPECT_TRUE(std::holds_alternative<std::vector<std::string>>(value));
    auto& list = std::get<std::vector<std::string>>(value);
    EXPECT_EQ(list.size(), 3);
    EXPECT_EQ(list[0], "a");
}

TEST(OptionDefTest, DefaultValues)
{
    OptionDef def;
    def.longName = "verbose";
    def.type = OptionValueType::Bool;
    EXPECT_FALSE(def.required);
    EXPECT_FALSE(def.shortName.has_value());
    EXPECT_FALSE(def.defaultValue.has_value());
    EXPECT_TRUE(def.choices.empty());
}

TEST(CommandSpecTest, SubcommandsAreEmpty)
{
    CommandSpec spec;
    spec.name = "build";
    spec.description = "Build the project";
    EXPECT_TRUE(spec.subcommands.empty());
}

TEST(CommandEntryTest, CanHoldSubcommands)
{
    CommandEntry child;
    child.spec.name = "install";
    child.source = CommandSource::Builtin;
    child.createHandler = [](const ParsedOptions&) {
        return nullptr;
    };

    CommandEntry parent;
    parent.spec.name = "toolchain";
    parent.source = CommandSource::Builtin;
    parent.createHandler = [](const ParsedOptions&) {
        return nullptr;
    };
    parent.subcommands.push_back(std::move(child));

    EXPECT_EQ(parent.subcommands.size(), 1);
    EXPECT_EQ(parent.subcommands[0].spec.name, "install");
}

TEST(ParseResultTest, SuccessCase)
{
    ParseResult result = CommandInvocation{"build", {}};
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->commandPath, "build");
}

TEST(ParseResultTest, HelpDirective)
{
    ParseResult result = std::unexpected(ParseInterruption{ParseDirective{ParseDirectiveKind::HelpRequested, "build"}});
    EXPECT_FALSE(result.has_value());

    auto& interruption = result.error();
    EXPECT_TRUE(std::holds_alternative<ParseDirective>(interruption));
    auto& directive = std::get<ParseDirective>(interruption);
    EXPECT_EQ(directive.kind, ParseDirectiveKind::HelpRequested);
    EXPECT_EQ(directive.target.value(), "build");
}

TEST(ParseResultTest, VersionDirective)
{
    ParseResult result =
        std::unexpected(ParseInterruption{ParseDirective{ParseDirectiveKind::VersionRequested, std::nullopt}});
    EXPECT_FALSE(result.has_value());

    auto& directive = std::get<ParseDirective>(result.error());
    EXPECT_EQ(directive.kind, ParseDirectiveKind::VersionRequested);
    EXPECT_FALSE(directive.target.has_value());
}

TEST(ParseResultTest, FailureCase)
{
    ParseResult result = std::unexpected(ParseInterruption{ParseFailure{"Unknown command: nonexistent"}});
    EXPECT_FALSE(result.has_value());

    auto& interruption = result.error();
    EXPECT_TRUE(std::holds_alternative<ParseFailure>(interruption));
    EXPECT_EQ(std::get<ParseFailure>(interruption).message, "Unknown command: nonexistent");
}

TEST(ParsedOptionsTest, NamedAndPositional)
{
    ParsedOptions opts;
    opts.named["verbose"] = true;
    opts.named["count"] = std::int64_t{3};
    opts.positional = {"arg1", "arg2"};

    EXPECT_TRUE(std::get<bool>(opts.named.at("verbose")));
    EXPECT_EQ(std::get<std::int64_t>(opts.named.at("count")), 3);
    EXPECT_EQ(opts.positional.size(), 2);
}

TEST(RuntimeEnvironmentTest, PathConstruction)
{
    RuntimeEnvironment env;
    env.projectRoot = "/tmp/project";
    env.searchPaths = {"/usr/local/bin", "/usr/bin"};

    EXPECT_EQ(env.projectRoot, "/tmp/project");
    EXPECT_EQ(env.searchPaths.size(), 2);
}

TEST(CommandSourceTest, EnumValues)
{
    EXPECT_NE(CommandSource::Builtin, CommandSource::External);
    EXPECT_NE(CommandSource::External, CommandSource::Project);
    EXPECT_NE(CommandSource::Builtin, CommandSource::Project);
}
