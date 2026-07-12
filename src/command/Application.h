#pragma once

#include "command/CommandResolver.h"
#include "command/HelpRenderer.h"
#include "command/ParseResult.h"
#include "command/ParserAdapter.h"
#include "command/VersionRenderer.h"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace scrap::Command {

class CommandCatalog;

/**
 * @brief Unified application class managing the CLI pipeline.
 *
 * Orchestrates the four-phase execution loop:
 * 1. Resolve  — collect CommandEntry trees from all registered Resolvers
 * 2. Configure — derive CommandSpec tree and configure the parser
 * 3. Parse    — parse argv into a ParseResult
 * 4. Execute  — dispatch to the matched handler, or handle directives/errors
 *
 * Resolver registration order determines command priority (first wins).
 */
class Application {
public:
    /**
     * @brief Construct with the parser and renderer dependencies.
     *
     * @param parser          CLI parser adapter (ownership transferred).
     * @param helpRenderer    Help renderer (ownership transferred).
     * @param versionRenderer Version renderer (ownership transferred).
     */
    Application(std::unique_ptr<ParserAdapter> parser,
                std::unique_ptr<HelpRenderer> helpRenderer,
                std::unique_ptr<VersionRenderer> versionRenderer);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) noexcept;
    Application& operator=(Application&&) noexcept;

    /**
     * @brief Register a command resolver.
     *
     * Resolvers are invoked in registration order during the Resolve phase.
     * The first resolver to register a command name wins on collision.
     *
     * @param resolver Resolver to add (ownership transferred).
     */
    auto addResolver(std::unique_ptr<CommandResolver> resolver) -> void;

    /**
     * @brief Execute the four-phase CLI pipeline.
     *
     * @param argv Raw argument vector (argv[0] is the program name).
     * @param env  Runtime environment with project root and search paths.
     * @return Exit code (0 for success, non-zero for failure).
     */
    auto run(std::span<const char* const> argv, const RuntimeEnvironment& env) -> int;

private:
    /**
     * @brief Handle a ParseDirective (help or version request).
     */
    auto handleDirective(const CommandCatalog& catalog, const ParseDirective& directive) -> int;

    /**
     * @brief Render help for a specific command or the global listing.
     */
    auto handleHelp(const CommandCatalog& catalog, const std::optional<std::string>& target) -> int;

    /**
     * @brief Handle a ParseFailure (error message + help suggestion).
     */
    static auto handleFailure(const ParseFailure& failure) -> int;

    std::unique_ptr<ParserAdapter> parser_;
    std::unique_ptr<HelpRenderer> helpRenderer_;
    std::unique_ptr<VersionRenderer> versionRenderer_;
    std::vector<std::unique_ptr<CommandResolver>> resolvers_;
};

}  // namespace scrap::Command
