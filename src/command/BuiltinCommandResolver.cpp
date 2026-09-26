#include "command/BuiltinCommandResolver.h"

#include "command/BuildCommandHandler.h"
#include "command/CleanCommandHandler.h"
#include "command/CommandEntry.h"
#include "command/CommandHandler.h"
#include "command/CommandSource.h"
#include "command/HelpRenderer.h"
#include "command/HelpRequest.h"
#include "command/InvocationContext.h"
#include "command/NewCommandHandler.h"
#include "command/OptionSchema.h"
#include "command/ParsedOptions.h"
#include "command/RuntimeEnvironment.h"
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
        std::optional<std::string_view> target;
        if (! ctx.options.positional.empty()) {
            target = ctx.options.positional[0];
        }
        return showHelp(*_renderer, *ctx.catalog, target);
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
 * Create the entry for "build", which takes an optional path into the project.
 */
CommandEntry makeBuildEntry()
{
    return makeProjectEntry(
        "build",
        "Compile the project",
        PositionalDef{ .name = "path", .description = "Directory inside the project (default: the current directory)", .required = false },
        [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<BuildCommandHandler>();
    });
}

/**
 * Create the entry for "clean", which takes an optional path into the project.
 */
CommandEntry makeCleanEntry()
{
    return makeProjectEntry(
        "clean",
        "Remove the build directory",
        PositionalDef{ .name = "path", .description = "Directory inside the project (default: the current directory)", .required = false },
        [](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
        return std::make_unique<CleanCommandHandler>();
    });
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
            PositionalDef{ .name = "command", .description = "Command to get help for", .required = false });
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
    entries.push_back(makePlaceholder("run", "Run the current project executable", "Project Commands"));
    entries.push_back(makeCleanEntry());

    // Toolchain commands
    {
        auto toolchain = makePlaceholder("toolchain", "Manage toolchains", "Toolchain Commands");
        toolchain.subcommands.push_back(makePlaceholder("list", "Display installed toolchains", "Toolchain Commands"));
        toolchain.subcommands.push_back(makePlaceholder("install", "Install a new toolchain", "Toolchain Commands"));
        toolchain.subcommands.push_back(makePlaceholder("select", "Select a toolchain as the default", "Toolchain Commands"));
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
