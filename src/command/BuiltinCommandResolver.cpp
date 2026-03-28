#include "command/BuiltinCommandResolver.h"

#include "command/CommandCatalog.h"
#include "command/CommandHandler.h"
#include "command/InvocationContext.h"

#include <iostream>
#include <memory>
#include <string>

namespace scrap::Command {

namespace {

/**
 * @brief Minimal CommandHandler that prints a message and returns 0.
 *
 * Used as a placeholder for domain commands whose real handlers
 * will be wired during the main.cpp migration.
 */
class PlaceholderHandler : public CommandHandler {
public:
    explicit PlaceholderHandler(std::string commandName)
        : commandName_(std::move(commandName))
    {
    }

    auto execute([[maybe_unused]] const InvocationContext& ctx) -> int override
    {
        std::cout << commandName_ << ": not yet implemented\n";
        return 0;
    }

private:
    std::string commandName_;
};

/**
 * @brief Handler for the built-in "help" command.
 */
class HelpCommandHandler : public CommandHandler {
public:
    explicit HelpCommandHandler(HelpRenderer& renderer)
        : renderer_(renderer)
    {
    }

    auto execute(const InvocationContext& ctx) -> int override
    {
        if (!ctx.options.positional.empty()) {
            auto target = ctx.options.positional[0];
            auto* entry = ctx.catalog.find(target);
            if (entry != nullptr) {
                auto specs = ctx.catalog.specs();
                for (const auto& spec : specs) {
                    if (spec.name == target) {
                        std::cout << renderer_.renderCommand(spec);
                        return 0;
                    }
                }
            }
            std::cerr << "Unknown command: " << target << "\n";
            return 1;
        }
        std::cout << renderer_.renderGlobal(ctx.catalog.helpEntries());
        return 0;
    }

private:
    HelpRenderer& renderer_;
};

/**
 * @brief Handler for the built-in "version" command.
 */
class VersionCommandHandler : public CommandHandler {
public:
    explicit VersionCommandHandler(VersionRenderer& renderer)
        : renderer_(renderer)
    {
    }

    auto execute([[maybe_unused]] const InvocationContext& ctx) -> int override
    {
        std::cout << renderer_.render() << "\n";
        return 0;
    }

private:
    VersionRenderer& renderer_;
};

/**
 * @brief Create a placeholder CommandEntry with a no-op handler.
 */
auto makePlaceholder(const std::string& name, const std::string& description, const std::string& category)
    -> CommandEntry
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

}  // anonymous namespace

BuiltinCommandResolver::BuiltinCommandResolver(HelpRenderer& helpRenderer, VersionRenderer& versionRenderer)
    : helpRenderer_(helpRenderer), versionRenderer_(versionRenderer)
{
}

auto BuiltinCommandResolver::resolve([[maybe_unused]] const RuntimeEnvironment& env) -> std::vector<CommandEntry>
{
    std::vector<CommandEntry> entries;

    // help command
    {
        CommandEntry entry;
        entry.spec.name = "help";
        entry.spec.description = "Display help information";
        entry.spec.category = "Built-in Commands";
        entry.spec.options.positional.push_back(
            PositionalDef{.name = "command", .description = "Command to get help for", .required = false});
        entry.source = CommandSource::Builtin;
        auto& renderer = helpRenderer_;
        entry.createHandler = [&renderer](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
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
        auto& renderer = versionRenderer_;
        entry.createHandler = [&renderer](const ParsedOptions&) -> std::unique_ptr<CommandHandler> {
            return std::make_unique<VersionCommandHandler>(renderer);
        };
        entries.push_back(std::move(entry));
    }

    // Project commands (placeholders)
    entries.push_back(makePlaceholder("new", "Create a new C++ project", "Project Commands"));
    entries.push_back(makePlaceholder("build", "Compile the current project", "Project Commands"));
    entries.push_back(makePlaceholder("run", "Run the current project executable", "Project Commands"));
    entries.push_back(makePlaceholder("clean", "Remove build artifacts and cached files", "Project Commands"));

    // Toolchain commands
    {
        auto toolchain = makePlaceholder("toolchain", "Manage toolchains", "Toolchain Commands");
        toolchain.subcommands.push_back(makePlaceholder("list", "Display installed toolchains", "Toolchain Commands"));
        toolchain.subcommands.push_back(makePlaceholder("install", "Install a new toolchain", "Toolchain Commands"));
        toolchain.subcommands.push_back(
            makePlaceholder("select", "Select a toolchain as the default", "Toolchain Commands"));
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
