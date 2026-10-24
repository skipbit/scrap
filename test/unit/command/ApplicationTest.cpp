#include "command/Application.h"

#include "command/CommandCatalog.h"
#include "command/CommandHandler.h"
#include "command/InvocationContext.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace scrap::Command;

namespace {

/**
 * Mock ParserAdapter that returns a configurable ParseResult.
 */
class MockParserAdapter : public ParserAdapter {
public:
    /**
     * Record the names of the configured commands.
     */
    void configure(std::span<const CommandSpec> specs) override
    {
        _configured.clear();
        for (const auto& spec : specs) {
            _configured.push_back(spec.name);
        }
    }

    /**
     * Return the names of the commands last configured.
     */
    [[nodiscard]] const std::vector<std::string>& configured() const
    {
        return _configured;
    }

    /**
     * Set the result to return from parse().
     */
    void setResult(ParseResult result)
    {
        _result = std::move(result);
    }

    /**
     * Return the pre-configured ParseResult.
     */
    [[nodiscard]] ParseResult parse([[maybe_unused]] std::span<const char* const> argv) const override
    {
        return _result;
    }

    /**
     * Refuse @p name from now on.
     */
    void refuseName(std::string name)
    {
        _refused.push_back(std::move(name));
    }

    /**
     * Accept every name not refused.
     */
    [[nodiscard]] bool acceptsName(std::string_view name) const override
    {
        return std::ranges::find(_refused, name) == _refused.end();
    }

private:
    ParseResult _result = std::unexpected(ParseInterruption{ ParseFailure{ "not configured" } });
    std::vector<std::string> _configured;
    std::vector<std::string> _refused;
};

/**
 * Mock HelpRenderer that records calls.
 */
class MockHelpRenderer : public HelpRenderer {
public:
    /**
     * Return a fixed global help string.
     */
    std::string renderGlobal([[maybe_unused]] std::span<const HelpEntry> entries) const override
    {
        return "mock global help\n";
    }

    /**
     * Return a fixed command help string including the command path and name.
     */
    std::string renderCommand(std::string_view path, const CommandSpec& spec) const override
    {
        return "mock help for: " + std::string{ path } + " (" + spec.name + ")\n";
    }
};

/**
 * Mock VersionRenderer that returns a fixed version string.
 */
class MockVersionRenderer : public VersionRenderer {
public:
    /**
     * Return a test version string.
     */
    std::string render() const override
    {
        return "scrap 0.0.1-test";
    }
};

/**
 * Mock CommandResolver that returns configurable entries.
 */
class MockResolver : public CommandResolver {
public:
    /**
     * Set the entries to return from resolve().
     */
    void setEntries(std::vector<CommandEntry> entries)
    {
        _entries = std::move(entries);
    }

    /**
     * Return the pre-configured entry list.
     */
    std::vector<CommandEntry> resolve([[maybe_unused]] const RuntimeEnvironment& env) override
    {
        return std::move(_entries);
    }

private:
    std::vector<CommandEntry> _entries;
};

/**
 * Stub CommandHandler that records execution and returns a fixed exit code.
 */
class StubHandler : public CommandHandler {
public:
    /**
     * Construct with the exit code to return.
     */
    explicit StubHandler(int exitCode)
        : _exitCode(exitCode)
    {
    }

    /**
     * Record execution and return the pre-configured exit code.
     */
    int execute([[maybe_unused]] const InvocationContext& ctx) override
    {
        _executed = true;
        return _exitCode;
    }

    /**
     * Check whether execute() was called.
     */
    [[nodiscard]] bool wasExecuted() const
    {
        return _executed;
    }

private:
    int _exitCode;
    bool _executed = false;
};

/**
 * Build a CommandEntry with a StubHandler returning the given exit code.
 */
CommandEntry makeEntry(const std::string& name, int exitCode = 0)
{
    CommandEntry entry;
    entry.spec.name = name;
    entry.spec.description = "Test command " + name;
    entry.source = CommandSource::Builtin;
    entry.createHandler = [exitCode](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<StubHandler>(exitCode);
    };
    return entry;
}

/**
 * Build a CommandEntry with subcommands.
 */
CommandEntry makeEntryWithSubs(const std::string& name, std::vector<CommandEntry> subs)
{
    auto entry = makeEntry(name);
    entry.subcommands = std::move(subs);
    return entry;
}

/**
 * Capture stdout during a callable execution.
 */
class StdoutCapture {
public:
    /**
     * Start capturing stdout.
     */
    StdoutCapture()
        : _original(std::cout.rdbuf(_captured.rdbuf()))
    {
    }

    /**
     * Stop capturing and restore stdout.
     */
    ~StdoutCapture()
    {
        std::cout.rdbuf(_original);
    }

    StdoutCapture(const StdoutCapture&) = delete;
    StdoutCapture& operator=(const StdoutCapture&) = delete;

    /**
     * Return captured output as a string.
     */
    [[nodiscard]] std::string str() const
    {
        return _captured.str();
    }

private:
    std::ostringstream _captured;
    std::streambuf* _original;
};

/**
 * Capture stderr during a callable execution.
 */
class StderrCapture {
public:
    /**
     * Start capturing stderr.
     */
    StderrCapture()
        : _original(std::cerr.rdbuf(_captured.rdbuf()))
    {
    }

    /**
     * Stop capturing and restore stderr.
     */
    ~StderrCapture()
    {
        std::cerr.rdbuf(_original);
    }

    StderrCapture(const StderrCapture&) = delete;
    StderrCapture& operator=(const StderrCapture&) = delete;

    /**
     * Return captured output as a string.
     */
    [[nodiscard]] std::string str() const
    {
        return _captured.str();
    }

private:
    std::ostringstream _captured;
    std::streambuf* _original;
};

}  // namespace

/**
 * Happy path: parsed command is found and handler is executed.
 */
TEST(ApplicationTest, Run_HappyPath_ExecutesHandler)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "build" }, {} });

    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntry("build", 0) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "build" };
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 0);
}

/**
 * Handler returning non-zero exit code is propagated.
 */
TEST(ApplicationTest, Run_HandlerExitCode_Propagated)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "build" }, {} });

    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntry("build", 42) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "build" };
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 42);
}

/**
 * Subcommand path is resolved correctly through the catalog.
 */
TEST(ApplicationTest, Run_SubcommandPath)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "toolchain", "install" }, {} });

    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntryWithSubs("toolchain", { makeEntry("install", 0) }) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "toolchain", "install" };
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 0);
}

/**
 * A command whose name holds a dot is found by its whole name.
 */
TEST(ApplicationTest, Run_NameWithADot)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "foo.sh" }, {} });

    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntry("foo.sh", 7) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "foo.sh" };
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 7);
}

/**
 * An external command whose name the parser refuses is left out with a
 * warning naming where it was found, and the other commands still run.
 */
TEST(ApplicationTest, Run_LeavesOutAnExternalCommandTheParserRefuses)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "build" }, {} });
    parser->refuseName("a:b");
    const auto* configured = parser.get();

    auto external = makeEntry("a:b");
    external.source = CommandSource::External;
    external.location = "/opt/bin/scrap-a:b";
    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntry("build", 3), std::move(external) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "build" };
    const StderrCapture captured;
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 3);
    EXPECT_EQ(configured->configured(), std::vector<std::string>{ "build" });
    EXPECT_EQ(captured.str(), "warning: command 'a:b' cannot be used as a command name; ignoring /opt/bin/scrap-a:b\n");
}

/**
 * Global help request renders global help and returns 0.
 */
TEST(ApplicationTest, Run_GlobalHelp)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(std::unexpected(ParseInterruption{ ParseDirective{ ParseDirectiveKind::HelpRequested, {} } }));

    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntry("build") });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "--help" };

    StdoutCapture capture;
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(capture.str().find("mock global help"), std::string::npos);
}

/**
 * Command help request renders help for the specific command.
 */
TEST(ApplicationTest, Run_CommandHelp)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(std::unexpected(ParseInterruption{ ParseDirective{ ParseDirectiveKind::HelpRequested, { "build" } } }));

    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntry("build") });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "build", "--help" };

    StdoutCapture capture;
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 0);
    EXPECT_EQ(capture.str(), "mock help for: build (build)\n");
}

/**
 * Help request for an unknown command returns error.
 */
TEST(ApplicationTest, Run_HelpUnknownCommand)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(
        std::unexpected(ParseInterruption{ ParseDirective{ ParseDirectiveKind::HelpRequested, { "nonexistent" } } }));

    Application app(std::make_unique<MockParserAdapter>(), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());

    // Use the pre-configured parser
    auto actualParser = std::make_unique<MockParserAdapter>();
    actualParser->setResult(
        std::unexpected(ParseInterruption{ ParseDirective{ ParseDirectiveKind::HelpRequested, { "nonexistent" } } }));

    Application app2(std::move(actualParser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "nonexistent", "--help" };

    StderrCapture capture;
    auto exitCode = app2.run(argv, env);

    EXPECT_EQ(exitCode, 2);
    EXPECT_EQ(capture.str(), "error: unknown command 'nonexistent'\nhint: run 'scrap --help' to list the commands\n");
}

namespace {

/**
 * Run an application with a parser that asks for the help of @p target, with
 * "toolchain list" and "toolchain install" registered, and return what it
 * wrote to standard output and standard error with its exit code.
 */
struct HelpOutcome {
    int exitCode = 0;
    std::string out;
    std::string err;
};

HelpOutcome runHelpFor(std::vector<std::string> target)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(std::unexpected(ParseInterruption{ ParseDirective{ ParseDirectiveKind::HelpRequested, std::move(target) } }));
    auto resolver = std::make_unique<MockResolver>();
    resolver->setEntries({ makeEntryWithSubs("toolchain", { makeEntry("list"), makeEntry("install") }) });
    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "--help" };
    StdoutCapture out;
    StderrCapture err;
    const int exitCode = app.run(argv, env);
    return HelpOutcome{ .exitCode = exitCode, .out = out.str(), .err = err.str() };
}

}  // namespace

/**
 * A nested command is looked up by its path and named by it.
 */
TEST(ApplicationTest, Run_NestedCommandHelp)
{
    const HelpOutcome outcome = runHelpFor({ "toolchain", "list" });

    EXPECT_EQ(outcome.exitCode, 0);
    EXPECT_EQ(outcome.out, "mock help for: toolchain list (list)\n");
    EXPECT_EQ(outcome.err, "");
}

/**
 * A path that leaves a command with subcommands points at its list.
 */
TEST(ApplicationTest, Run_HelpUnknownNestedCommand)
{
    const HelpOutcome outcome = runHelpFor({ "toolchain", "nosuch" });

    EXPECT_EQ(outcome.exitCode, 2);
    EXPECT_EQ(outcome.out, "");
    EXPECT_EQ(outcome.err,
              "error: unknown command 'toolchain nosuch'\n"
              "hint: run 'scrap toolchain --help' to list its commands\n");
}

/**
 * A path that goes on past a command without subcommands points at its usage.
 */
TEST(ApplicationTest, Run_HelpPastACommandWithoutSubcommands)
{
    const HelpOutcome outcome = runHelpFor({ "toolchain", "list", "nosuch" });

    EXPECT_EQ(outcome.exitCode, 2);
    EXPECT_EQ(outcome.out, "");
    EXPECT_EQ(outcome.err,
              "error: unknown command 'toolchain list nosuch'\n"
              "hint: run 'scrap toolchain list --help' for its usage\n");
}

/**
 * A path whose first word names no command points at the global help.
 */
TEST(ApplicationTest, Run_HelpUnknownPathFromTheTop)
{
    const HelpOutcome outcome = runHelpFor({ "nosuch", "list" });

    EXPECT_EQ(outcome.exitCode, 2);
    EXPECT_EQ(outcome.err, "error: unknown command 'nosuch list'\nhint: run 'scrap --help' to list the commands\n");
}

/**
 * Version request outputs version string and returns 0.
 */
TEST(ApplicationTest, Run_VersionRequested)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(std::unexpected(ParseInterruption{ ParseDirective{ ParseDirectiveKind::VersionRequested, {} } }));

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "--version" };

    StdoutCapture capture;
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(capture.str().find("scrap 0.0.1-test"), std::string::npos);
}

/**
 * Parse failure outputs error message and returns 1.
 */
TEST(ApplicationTest, Run_ParseFailure)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(std::unexpected(ParseInterruption{ ParseFailure{ "Unknown command: nonexistent" } }));

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "nonexistent" };

    StderrCapture capture;
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 1);
    EXPECT_NE(capture.str().find("Unknown command: nonexistent"), std::string::npos);
    EXPECT_NE(capture.str().find("scrap --help"), std::string::npos);
}

/**
 * The parser writes what the user typed into its message: an escape sequence
 * in it reaches the terminal as text, and letters stay as they are.
 */
TEST(ApplicationTest, Run_ParseFailureWritesTheMessageAsText)
{
    auto parser = std::make_unique<MockParserAdapter>();
    // The argument holds U+65E5 and an escape sequence that names a terminal.
    parser->setResult(
        std::unexpected(ParseInterruption{ ParseFailure{ "The following argument was not expected: \xe6\x97\xa5x\x1b]0;pwn\x07" } }));

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "new", "hello" };

    StderrCapture capture;
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 1);
    EXPECT_NE(capture.str().find("The following argument was not expected: \xe6\x97\xa5x\\x1B]0;pwn\\x07"), std::string::npos);
    EXPECT_EQ(capture.str().find('\x1b'), std::string::npos);
    EXPECT_EQ(capture.str().find('\x07'), std::string::npos);
}

/**
 * Multiple resolvers merge entries; first resolver wins on collision.
 */
TEST(ApplicationTest, Run_ResolverPriority)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "build" }, {} });

    auto resolver1 = std::make_unique<MockResolver>();
    resolver1->setEntries({ makeEntry("build", 10) });

    auto resolver2 = std::make_unique<MockResolver>();
    resolver2->setEntries({ makeEntry("build", 20) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(resolver1));
    app.addResolver(std::move(resolver2));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "build" };

    StderrCapture stderrCapture;
    auto exitCode = app.run(argv, env);

    // First resolver's exit code (10) should win.
    EXPECT_EQ(exitCode, 10);
}

/**
 * Commands from multiple resolvers are all accessible.
 */
TEST(ApplicationTest, Run_MultipleResolvers_MergedCatalog)
{
    auto parser = std::make_unique<MockParserAdapter>();
    parser->setResult(CommandInvocation{ { "lint" }, {} });

    auto builtinResolver = std::make_unique<MockResolver>();
    builtinResolver->setEntries({ makeEntry("build", 0) });

    auto externalResolver = std::make_unique<MockResolver>();
    externalResolver->setEntries({ makeEntry("lint", 0) });

    Application app(std::move(parser), std::make_unique<MockHelpRenderer>(), std::make_unique<MockVersionRenderer>());
    app.addResolver(std::move(builtinResolver));
    app.addResolver(std::move(externalResolver));

    RuntimeEnvironment env;
    const char* argv[] = { "scrap", "lint" };
    auto exitCode = app.run(argv, env);

    EXPECT_EQ(exitCode, 0);
}
