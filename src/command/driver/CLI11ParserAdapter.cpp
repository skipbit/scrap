#include "command/driver/CLI11ParserAdapter.h"

#include "command/CommandSpec.h"
#include "command/OptionSchema.h"
#include "command/ParseResult.h"
#include "command/ParsedOptions.h"

#include <CLI/CLI.hpp>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace scrap::Command {

// =============================================================================
// Storage helpers — allocated per parse() call so that parse() stays const.
// =============================================================================

/**
 * @brief Holds mutable value slots that CLI11 writes into during parsing.
 *
 * One instance is created per CommandSpec node in the spec tree.
 * After CLI11 finishes parsing, the values are harvested into a
 * ParsedOptions struct.
 *
 * Positionals use std::deque so that push_back never invalidates
 * the references that CLI11 holds to earlier elements.
 */
struct OptionStorage {
    std::unordered_map<std::string, bool> bools;
    std::unordered_map<std::string, std::int64_t> ints;
    std::unordered_map<std::string, std::string> strings;
    std::unordered_map<std::string, std::vector<std::string>> stringLists;
    std::deque<std::string> positionals;
};

// =============================================================================
// Impl
// =============================================================================

class CLI11ParserAdapter::Impl {
public:
    std::vector<CommandSpec> specs_;
};

// =============================================================================
// Free helpers (internal linkage)
// =============================================================================

namespace {

/**
 * @brief Register one OptionDef as a CLI11 option or flag.
 *
 * Writes the parsed value into @p storage so it can be harvested
 * after CLI11 returns.
 */
void addOption(CLI::App& app, const OptionDef& def, OptionStorage& storage)
{
    // Build the flag/option name string (e.g. "--verbose,-v").
    std::string nameStr = "--" + def.longName;
    if (def.shortName.has_value()) {
        nameStr += ",-";
        nameStr += *def.shortName;
    }

    switch (def.type) {
        case OptionValueType::Bool: {
            storage.bools[def.longName] = false;
            app.add_flag(nameStr, storage.bools[def.longName], def.description);
            break;
        }
        case OptionValueType::Int64: {
            storage.ints[def.longName] = 0;
            auto* opt = app.add_option(nameStr, storage.ints[def.longName], def.description);
            if (def.required) {
                opt->required();
            }
            if (def.defaultValue.has_value()) {
                opt->default_val(std::get<std::int64_t>(*def.defaultValue));
            }
            break;
        }
        case OptionValueType::String: {
            storage.strings[def.longName] = "";
            auto* opt = app.add_option(nameStr, storage.strings[def.longName], def.description);
            if (def.required) {
                opt->required();
            }
            if (def.defaultValue.has_value()) {
                opt->default_val(std::get<std::string>(*def.defaultValue));
            }
            if (!def.choices.empty()) {
                opt->check(CLI::IsMember(def.choices));
            }
            break;
        }
        case OptionValueType::StringList: {
            storage.stringLists[def.longName] = {};
            auto* opt = app.add_option(nameStr, storage.stringLists[def.longName], def.description);
            opt->expected(-1);
            if (def.required) {
                opt->required();
            }
            break;
        }
    }
}

/**
 * @brief Register one PositionalDef as a CLI11 positional argument.
 */
void addPositional(CLI::App& app, const PositionalDef& def, OptionStorage& storage)
{
    storage.positionals.emplace_back();
    auto& slot = storage.positionals.back();
    auto* opt = app.add_option(def.name, slot, def.description);
    if (def.required) {
        opt->required();
    }
}

/**
 * @brief Recursively map a CommandSpec tree onto CLI11 subcommands.
 *
 * @param parent      CLI11 app or subcommand to attach children to.
 * @param specs       CommandSpec nodes at the current tree level.
 * @param storageMap  Flat map from dot-path → OptionStorage (owned by caller).
 * @param prefix      Dot-separated path prefix for the current level.
 */
void addSubcommands(CLI::App& parent,
                    const std::vector<CommandSpec>& specs,
                    std::unordered_map<std::string, std::unique_ptr<OptionStorage>>& storageMap,
                    const std::string& prefix)
{
    for (const auto& spec : specs) {
        auto* sub = parent.add_subcommand(spec.name, spec.description);

        // Build the dot-separated key for this node.
        std::string path = prefix.empty() ? spec.name : (prefix + "." + spec.name);

        // Create option storage for this command node.
        auto storage = std::make_unique<OptionStorage>();

        for (const auto& opt : spec.options.named) {
            addOption(*sub, opt, *storage);
        }
        for (const auto& pos : spec.options.positional) {
            addPositional(*sub, pos, *storage);
        }

        storageMap[path] = std::move(storage);

        // Recurse into subcommands.
        if (!spec.subcommands.empty()) {
            addSubcommands(*sub, spec.subcommands, storageMap, path);
        }
    }
}

/**
 * @brief Walk the parsed subcommand chain and build a dot-separated path.
 *
 * Starts from @p app and follows the first parsed child at each level.
 *
 * @return Dot-separated command path (e.g. "toolchain.install"), or
 *         empty string if no subcommand was parsed.
 */
auto buildCommandPath(const CLI::App& app) -> std::string
{
    std::string path;
    const CLI::App* current = &app;

    while (true) {
        const CLI::App* parsed = nullptr;
        for (const auto* sub : current->get_subcommands()) {
            if (sub->parsed()) {
                parsed = sub;
                break;
            }
        }

        if (parsed == nullptr) {
            break;
        }

        if (!path.empty()) {
            path += '.';
        }
        path += parsed->get_name();
        current = parsed;
    }

    return path;
}

/**
 * @brief Harvest parsed values from OptionStorage into ParsedOptions.
 */
auto harvestOptions(const OptionStorage& storage) -> ParsedOptions
{
    ParsedOptions opts;

    for (const auto& [name, value] : storage.bools) {
        opts.named[name] = value;
    }
    for (const auto& [name, value] : storage.ints) {
        opts.named[name] = value;
    }
    for (const auto& [name, value] : storage.strings) {
        if (!value.empty()) {
            opts.named[name] = value;
        }
    }
    for (const auto& [name, value] : storage.stringLists) {
        if (!value.empty()) {
            opts.named[name] = value;
        }
    }

    opts.positional.assign(storage.positionals.begin(), storage.positionals.end());
    return opts;
}

/**
 * @brief Determine the help target from the parsed subcommand chain.
 *
 * When CLI11 throws CallForHelp, we need to know which subcommand
 * the user asked help for (e.g. "scrap build --help" → target "build").
 *
 * @return The command path if a subcommand was at least partially
 *         parsed, or nullopt for global help.
 */
auto determineHelpTarget(const CLI::App& app) -> std::optional<std::string>
{
    auto path = buildCommandPath(app);
    if (path.empty()) {
        return std::nullopt;
    }
    return path;
}

}  // anonymous namespace

// =============================================================================
// CLI11ParserAdapter — public interface
// =============================================================================

CLI11ParserAdapter::CLI11ParserAdapter()
    : impl_(std::make_unique<Impl>())
{
}

CLI11ParserAdapter::~CLI11ParserAdapter() = default;
CLI11ParserAdapter::CLI11ParserAdapter(CLI11ParserAdapter&&) noexcept = default;
CLI11ParserAdapter& CLI11ParserAdapter::operator=(CLI11ParserAdapter&&) noexcept = default;

// --- configure ---------------------------------------------------------------

auto CLI11ParserAdapter::configure(std::span<const CommandSpec> specs) -> void
{
    impl_->specs_.assign(specs.begin(), specs.end());
}

// --- parse -------------------------------------------------------------------

auto CLI11ParserAdapter::parse(std::span<const char* const> argv) const -> ParseResult
{
    // Build a temporary CLI::App from the stored specs.
    // Defined outside try so that catch blocks can inspect parsed state.
    CLI::App app{"Modern C++ development tool", "scrap"};
    app.set_version_flag("--version,-V", "");
    app.require_subcommand(1);

    // Per-call storage map: dot-path → OptionStorage.
    std::unordered_map<std::string, std::unique_ptr<OptionStorage>> storageMap;

    try {
        // Map the CommandSpec tree onto CLI11 subcommands and options.
        addSubcommands(app, impl_->specs_, storageMap, "");

        // --- Parse ---------------------------------------------------------------
        app.parse(static_cast<int>(argv.size()), argv.data());

    } catch (const CLI::CallForHelp&) {
        return std::unexpected(
            ParseInterruption{ParseDirective{ParseDirectiveKind::HelpRequested, determineHelpTarget(app)}});
    } catch (const CLI::CallForVersion&) {
        return std::unexpected(ParseInterruption{ParseDirective{ParseDirectiveKind::VersionRequested, std::nullopt}});
    } catch (const CLI::CallForAllHelp&) {
        return std::unexpected(ParseInterruption{ParseDirective{ParseDirectiveKind::HelpRequested, std::nullopt}});
    } catch (const CLI::ParseError& e) {
        return std::unexpected(ParseInterruption{ParseFailure{e.what()}});
    } catch (const std::exception& e) {
        // Catch remaining exceptions (e.g. bad_variant_access from
        // misconfigured OptionDef default values) and convert to failure.
        return std::unexpected(ParseInterruption{ParseFailure{e.what()}});
    }

    // --- Harvest results ---------------------------------------------------------
    auto commandPath = buildCommandPath(app);

    ParsedOptions options;
    auto storageIt = storageMap.find(commandPath);
    if (storageIt != storageMap.end()) {
        options = harvestOptions(*storageIt->second);
    }

    return CommandInvocation{std::move(commandPath), std::move(options)};
}

}  // namespace scrap::Command
