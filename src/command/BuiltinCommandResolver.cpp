#include "command/BuiltinCommandResolver.h"

#include "command/BuildCommandHandler.h"
#include "command/CleanCommandHandler.h"
#include "command/CommandEntry.h"
#include "command/CommandHandler.h"
#include "command/CommandSource.h"
#include "command/FormatCommandHandler.h"
#include "command/HelpRenderer.h"
#include "command/HelpRequest.h"
#include "command/InvocationContext.h"
#include "command/NewCommandHandler.h"
#include "command/OptionSchema.h"
#include "command/ParsedOptions.h"
#include "command/ProjectBuild.h"
#include "command/RunCommandHandler.h"
#include "command/RuntimeEnvironment.h"
#include "command/ToolchainInfoCommandHandler.h"
#include "command/ToolchainInstallCommandHandler.h"
#include "command/ToolchainListCommandHandler.h"
#include "command/ToolchainSelectCommandHandler.h"
#include "command/ToolchainUninstallCommandHandler.h"
#include "command/ToolchainUnselectCommandHandler.h"
#include "command/ToolchainUpdateCommandHandler.h"
#include "command/VersionRenderer.h"
#include "project/ProjectFileSystem.h"

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scrap::Command {

namespace {

/**
 * Minimal CommandHandler that prints a message and returns 0.
 * Used as a placeholder until real domain handlers are wired.
 */
class PlaceholderHandler : public CommandHandler {
public:
    /**
     * Construct with the command name shown in the placeholder message.
     */
    explicit PlaceholderHandler(std::string commandName)
        : _commandName(std::move(commandName))
    {
    }

    /**
     * Print a "not yet implemented" message and return success. It reports
     * the state of the command rather than answering it, so it goes to
     * standard error.
     */
    int execute([[maybe_unused]] const InvocationContext& ctx) override
    {
        std::cerr << _commandName << ": not yet implemented\n";
        return 0;
    }

private:
    std::string _commandName;
};

/**
 * Handler for the built-in "help" command.
 * Renders global or per-command help via the injected HelpRenderer.
 */
class HelpCommandHandler : public CommandHandler {
public:
    /**
     * Construct with a non-owning pointer to the shared HelpRenderer.
     */
    explicit HelpCommandHandler(HelpRenderer* renderer)
        : _renderer(renderer)
    {
    }

    /**
     * Render help for a specific command, or global help if no target given.
     */
    int execute(const InvocationContext& ctx) override
    {
        return showHelp(*_renderer, *ctx.catalog, ctx.options.positional);
    }

private:
    HelpRenderer* _renderer;
};

/**
 * Handler for the built-in "version" command.
 * Outputs the version string via the injected VersionRenderer.
 */
class VersionCommandHandler : public CommandHandler {
public:
    /**
     * Construct with a non-owning pointer to the shared VersionRenderer.
     */
    explicit VersionCommandHandler(VersionRenderer* renderer)
        : _renderer(renderer)
    {
    }

    /**
     * Print the version string and return success.
     */
    int execute([[maybe_unused]] const InvocationContext& ctx) override
    {
        std::cout << _renderer->render() << "\n";
        return 0;
    }

private:
    VersionRenderer* _renderer;
};

/**
 * Create a placeholder CommandEntry with a no-op handler.
 */
CommandEntry makePlaceholder(const std::string& name, const std::string& description, const std::string& category)
{
    CommandEntry entry;
    entry.spec.name = name;
    entry.spec.description = description;
    entry.spec.category = category;
    entry.source = CommandSource::Builtin;
    entry.createHandler = [name](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<PlaceholderHandler>(name);
    };
    return entry;
}

/**
 * Create a project command that takes one positional argument.
 */
CommandEntry makeProjectEntry(std::string name,
                              std::string description,
                              PositionalDef positional,
                              CommandEntry::HandlerFactory createHandler)
{
    CommandEntry entry;
    entry.spec.name = std::move(name);
    entry.spec.description = std::move(description);
    entry.spec.category = "Project Commands";
    entry.spec.options.positional.push_back(std::move(positional));
    entry.source = CommandSource::Builtin;
    entry.createHandler = std::move(createHandler);
    return entry;
}

/**
 * Create the entry for "new", which takes the name of the project to create.
 */
CommandEntry makeNewEntry(Project::ProjectFileSystem& fileSystem)
{
    auto* files = &fileSystem;
    return makeProjectEntry(
        "new",
        "Create a new C++ project",
        PositionalDef{ .name = "project-name", .description = "Name of the project directory to create", .required = true },
        [files](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<NewCommandHandler>(*files);
    });
}

/**
 * The optional path into the project that the project commands take.
 */
PositionalDef projectPathPositional()
{
    return PositionalDef{ .name = "path", .description = "Directory inside the project (default: the current directory)", .required = false };
}

/**
 * The flag that asks for the release profile, described as @p description.
 */
OptionDef releaseFlag(std::string description)
{
    return OptionDef{
        .longName = std::string{ ReleaseOption },
        .shortName = std::nullopt,
        .type = OptionValueType::Bool,
        .required = false,
        .description = std::move(description),
        .defaultValue = std::nullopt,
        .choices = {}
    };
}

/**
 * The option that sets how many steps of a build run at once.
 */
OptionDef jobsOption()
{
    return OptionDef{
        .longName = std::string{ JobsOption },
        .shortName = 'j',
        .type = OptionValueType::PositiveInt64,
        .required = false,
        .description = "Run this many build steps at once",
        .defaultValue = std::nullopt,
        .choices = {}
    };
}

/**
 * The option @p name that takes the name of a target, described as
 * @p description.
 */
OptionDef targetNameOption(const std::string_view name, std::string description)
{
    return OptionDef{
        .longName = std::string{ name },
        .shortName = std::nullopt,
        .type = OptionValueType::String,
        .required = false,
        .description = std::move(description),
        .defaultValue = std::nullopt,
        .choices = {}
    };
}

/**
 * Create the entry for "build", which takes an optional path into the project,
 * whether to build for release, the one target to build, and how many steps
 * run at once.
 */
CommandEntry makeBuildEntry()
{
    auto entry = makeProjectEntry("build", "Compile the project", projectPathPositional(), [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<BuildCommandHandler>();
    });
    entry.spec.options.named.push_back(releaseFlag("Build with the release profile"));
    entry.spec.options.named.push_back(targetNameOption(TargetOption, "Build only this target"));
    entry.spec.options.named.push_back(jobsOption());
    return entry;
}

/**
 * Create the entry for "clean", which takes an optional path into the project.
 */
CommandEntry makeCleanEntry()
{
    return makeProjectEntry(
        "clean", "Remove the build directory", projectPathPositional(), [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<CleanCommandHandler>();
    });
}

/**
 * Create the entry for "format", which takes the files and directories to
 * format, and whether to check them instead.
 */
CommandEntry makeFormatEntry()
{
    auto entry = makeProjectEntry(
        "format",
        "Format the sources of the project",
        PositionalDef{ .name = "path", .description = "File or directory to format (default: the whole project)", .required = false, .repeats = true },
        [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<FormatCommandHandler>();
    });
    entry.spec.options.named.push_back(OptionDef{ .longName = std::string{ CheckOption },
                                                  .shortName = std::nullopt,
                                                  .type = OptionValueType::Bool,
                                                  .required = false,
                                                  .description = "Report the files formatting would change, and change none",
                                                  .defaultValue = std::nullopt,
                                                  .choices = {} });
    return entry;
}

/**
 * Create the entry for "run", which takes an optional path into the project,
 * whether to build for release, the executable to run, how many steps of the
 * build run at once, and the program's arguments after "--".
 */
CommandEntry makeRunEntry()
{
    auto entry = makeProjectEntry(
        "run", "Build the project and run its executable", projectPathPositional(), [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<RunCommandHandler>();
    });
    entry.spec.options.named.push_back(releaseFlag("Build and run with the release profile"));
    entry.spec.options.named.push_back(targetNameOption(BinOption, "Run this executable"));
    entry.spec.options.named.push_back(jobsOption());
    entry.spec.options.trailing = TrailingDef{ .name = "args", .description = "Arguments passed to the program" };
    return entry;
}

/**
 * Create the entry for "toolchain install", which takes the specifier of the
 * toolchain to install.
 */
CommandEntry makeToolchainInstallEntry()
{
    CommandEntry entry;
    entry.spec.name = "install";
    entry.spec.description = "Install a toolchain";
    entry.spec.category = "Toolchain Commands";
    entry.spec.options.positional.push_back(
        PositionalDef{ .name = "specifier", .description = "Toolchain to install, as <family>[@<version>]", .required = true });
    entry.spec.options.named.push_back(OptionDef{
        .longName = std::string{ ForceOption },
        .shortName = std::nullopt,
        .type = OptionValueType::Bool,
        .required = false,
        .description = "Replace the toolchain when it is already installed",
        .defaultValue = std::nullopt,
        .choices = {} });
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainInstallCommandHandler>();
    };
    return entry;
}

/**
 * Create the entry for "toolchain select", which takes the specifier of the
 * toolchain to keep as the global default.
 */
CommandEntry makeToolchainSelectEntry()
{
    CommandEntry entry;
    entry.spec.name = "select";
    entry.spec.description = "Select a toolchain as the global default";
    entry.spec.category = "Toolchain Commands";
    entry.spec.options.positional.push_back(
        PositionalDef{ .name = "specifier", .description = "Toolchain to select, as <family>[@<version>] or system", .required = true });
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainSelectCommandHandler>();
    };
    return entry;
}

/**
 * Create the entry for "toolchain unselect", which removes the global
 * default.
 */
CommandEntry makeToolchainUnselectEntry()
{
    CommandEntry entry;
    entry.spec.name = "unselect";
    entry.spec.description = "Remove the global default toolchain";
    entry.spec.category = "Toolchain Commands";
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainUnselectCommandHandler>();
    };
    return entry;
}

/**
 * Create the entry for "toolchain update", which fetches the toolchain index
 * and tells what changed.
 */
CommandEntry makeToolchainUpdateEntry()
{
    CommandEntry entry;
    entry.spec.name = "update";
    entry.spec.description = "Fetch the toolchain index and show what changed";
    entry.spec.category = "Toolchain Commands";
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainUpdateCommandHandler>();
    };
    return entry;
}

CommandEntry makeToolchainListEntry()
{
    CommandEntry entry;
    entry.spec.name = "list";
    entry.spec.description = "Display the toolchains to choose from";
    entry.spec.category = "Toolchain Commands";
    entry.spec.options.named.push_back(OptionDef{ .longName = std::string{ RemoteOption },
                                                  .shortName = std::nullopt,
                                                  .type = OptionValueType::Bool,
                                                  .required = false,
                                                  .description = "Display the toolchains of the index the host can install, as last fetched",
                                                  .defaultValue = std::nullopt,
                                                  .choices = {} });
    entry.spec.options.named.push_back(OptionDef{ .longName = std::string{ AllOption },
                                                  .shortName = std::nullopt,
                                                  .type = OptionValueType::Bool,
                                                  .required = false,
                                                  .description = "With --remote, display every toolchain of the index, with why the host cannot install one",
                                                  .defaultValue = std::nullopt,
                                                  .choices = {} });
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainListCommandHandler>();
    };
    return entry;
}

/**
 * Create the entry for "toolchain info", which takes the specifier of the
 * toolchain to tell.
 */
CommandEntry makeToolchainInfoEntry()
{
    CommandEntry entry;
    entry.spec.name = "info";
    entry.spec.description = "Display the details of a toolchain";
    entry.spec.category = "Toolchain Commands";
    entry.spec.options.positional.push_back(
        PositionalDef{ .name = "specifier", .description = "Toolchain to display, as <family>[@<version>] or system", .required = true });
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainInfoCommandHandler>();
    };
    return entry;
}

CommandEntry makeToolchainUninstallEntry()
{
    CommandEntry entry;
    entry.spec.name = "uninstall";
    entry.spec.description = "Remove an installed toolchain";
    entry.spec.category = "Toolchain Commands";
    entry.spec.options.positional.push_back(
        PositionalDef{ .name = "specifier", .description = "Toolchain to remove, as <family>[@<version>] naming one installed version", .required = true });
    entry.source = CommandSource::Builtin;
    entry.createHandler = [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<ToolchainUninstallCommandHandler>();
    };
    return entry;
}

}  // anonymous namespace

/**
 * Construct with renderer pointers for help/version commands.
 */
BuiltinCommandResolver::BuiltinCommandResolver(HelpRenderer& helpRenderer,
                                               VersionRenderer& versionRenderer,
                                               Project::ProjectFileSystem& fileSystem)
    : _helpRenderer(&helpRenderer)
    , _versionRenderer(&versionRenderer)
    , _fileSystem(&fileSystem)
{
}

/**
 * Return the fixed set of built-in command entries.
 */
std::vector<CommandEntry> BuiltinCommandResolver::resolve([[maybe_unused]] const RuntimeEnvironment& env)
{
    std::vector<CommandEntry> entries;

    // help command
    {
        CommandEntry entry;
        entry.spec.name = "help";
        entry.spec.description = "Display help information";
        entry.spec.category = "Built-in Commands";
        entry.spec.options.positional.push_back(
            PositionalDef{ .name = "command", .description = "Command to get help for", .required = false, .repeats = true });
        entry.source = CommandSource::Builtin;
        auto* renderer = _helpRenderer;
        entry.createHandler = [renderer](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
            return std::make_unique<HelpCommandHandler>(renderer);
        };
        entries.push_back(std::move(entry));
    }

    // version command
    {
        CommandEntry entry;
        entry.spec.name = "version";
        entry.spec.description = "Display version information";
        entry.spec.category = "Built-in Commands";
        entry.source = CommandSource::Builtin;
        auto* renderer = _versionRenderer;
        entry.createHandler = [renderer](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
            return std::make_unique<VersionCommandHandler>(renderer);
        };
        entries.push_back(std::move(entry));
    }

    // Project commands
    entries.push_back(makeNewEntry(*_fileSystem));
    entries.push_back(makeBuildEntry());
    entries.push_back(makeRunEntry());
    entries.push_back(makeCleanEntry());
    entries.push_back(makeFormatEntry());

    // Toolchain commands
    {
        auto toolchain = makePlaceholder("toolchain", "Manage toolchains", "Toolchain Commands");
        toolchain.subcommands.push_back(makeToolchainListEntry());
        toolchain.subcommands.push_back(makeToolchainInfoEntry());
        toolchain.subcommands.push_back(makeToolchainInstallEntry());
        toolchain.subcommands.push_back(makeToolchainUninstallEntry());
        toolchain.subcommands.push_back(makeToolchainSelectEntry());
        toolchain.subcommands.push_back(makeToolchainUnselectEntry());
        toolchain.subcommands.push_back(makeToolchainUpdateEntry());
        entries.push_back(std::move(toolchain));
    }

    // Template commands
    {
        auto tmpl = makePlaceholder("template", "Manage project templates", "Template Commands");
        tmpl.subcommands.push_back(makePlaceholder("list", "List available templates", "Template Commands"));
        tmpl.subcommands.push_back(makePlaceholder("update", "Update template sources", "Template Commands"));
        entries.push_back(std::move(tmpl));
    }

    return entries;
}

}  // namespace scrap::Command
