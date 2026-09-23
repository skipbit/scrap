#pragma once

#include "command/CommandResolver.h"
#include "command/HelpRenderer.h"
#include "command/ParseResult.h"
#include "command/ParserAdapter.h"
#include "command/VersionRenderer.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace scrap::Command {

class CommandCatalog;

/**
 * @brief Unified application class managing the CLI pipeline.
 *
 * Orchestrates the four-phase execution loop:
 * 1. Resolve  - collect CommandEntry trees from all registered Resolvers
 * 2. Configure - derive CommandSpec tree and configure the parser
 * 3. Parse    - parse argv into a ParseResult
 * 4. Execute  - dispatch to the matched handler, or handle directives/errors
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
    void addResolver(std::unique_ptr<CommandResolver> resolver);

    /**
     * @brief Execute the four-phase CLI pipeline.
     *
     * @param argv Raw argument vector (argv[0] is the program name).
     * @param env  Runtime environment with working directory and search paths.
     * @return Exit code (0 for success, non-zero for failure).
     */
    int run(std::span<const char* const> argv, const RuntimeEnvironment& env);

private:
    /**
     * @brief Handle a ParseDirective (help or version request).
     */
    int handleDirective(const CommandCatalog& catalog, const ParseDirective& directive);

    /**
     * @brief Handle a ParseFailure (error message + help suggestion).
     */
    static int handleFailure(const ParseFailure& failure);

    std::unique_ptr<ParserAdapter> _parser;
    std::unique_ptr<HelpRenderer> _helpRenderer;
    std::unique_ptr<VersionRenderer> _versionRenderer;
    std::vector<std::unique_ptr<CommandResolver>> _resolvers;
};

}  // namespace scrap::Command
