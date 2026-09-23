#include "command/Application.h"

#include "command/CommandCatalog.h"
#include "command/CommandResolver.h"
#include "command/HelpRenderer.h"
#include "command/HelpRequest.h"
#include "command/InvocationContext.h"
#include "command/ParseResult.h"
#include "command/ParserAdapter.h"
#include "command/PrintableText.h"
#include "command/RuntimeEnvironment.h"
#include "command/VersionRenderer.h"

#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace scrap::Command {

/**
 * Construct with parser and renderer dependencies.
 */
Application::Application(std::unique_ptr<ParserAdapter> parser,
                         std::unique_ptr<HelpRenderer> helpRenderer,
                         std::unique_ptr<VersionRenderer> versionRenderer)
    : _parser(std::move(parser))
    , _helpRenderer(std::move(helpRenderer))
    , _versionRenderer(std::move(versionRenderer))
{
}

Application::~Application() = default;
Application::Application(Application&&) noexcept = default;
Application& Application::operator=(Application&&) noexcept = default;

/**
 * Register a command resolver for the Resolve phase.
 */
void Application::addResolver(std::unique_ptr<CommandResolver> resolver)
{
    _resolvers.push_back(std::move(resolver));
}

/**
 * Execute the four-phase CLI pipeline: Resolve, Configure, Parse, Execute.
 */
int Application::run(std::span<const char* const> argv, const RuntimeEnvironment& env)
{
    // Phase 1: Resolve - collect CommandEntry trees from all resolvers.
    CommandCatalog catalog;
    for (auto& resolver : _resolvers) {
        catalog.addEntries(resolver->resolve(env));
    }

    // Phase 2: Configure - derive CommandSpec tree and feed to parser.
    auto specTree = catalog.specs();
    _parser->configure(specTree);

    // Phase 3: Parse - parse argv into a ParseResult.
    auto result = _parser->parse(argv);

    // Phase 4: Execute - dispatch based on ParseResult.
    if (result.has_value()) {
        auto& invocation = *result;
        const auto* entry = catalog.find(invocation.commandPath);
        if (entry == nullptr) {
            std::cerr << "Internal error: command not found after parsing: " << invocation.commandPath << "\n";
            return 1;
        }
        auto handler = entry->createHandler(invocation.options);
        if (handler == nullptr) {
            std::cerr << "Command '" << invocation.commandPath << "' is not available yet.\n";
            std::cerr << "Run 'scrap --help' for usage information.\n";
            return 1;
        }
        const InvocationContext ctx{ invocation.options, &env, &catalog };
        return handler->execute(ctx);
    }

    return std::visit([&](const auto& value) -> int {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ParseDirective>) {
            return handleDirective(catalog, value);
        } else {
            return handleFailure(value);
        }
    }, result.error());
}

/**
 * Handle a ParseDirective (help or version request).
 */
int Application::handleDirective(const CommandCatalog& catalog, const ParseDirective& directive)
{
    switch (directive.kind) {
    case ParseDirectiveKind::HelpRequested:
        return showHelp(*_helpRenderer, catalog, directive.target);
    case ParseDirectiveKind::VersionRequested:
        std::cout << _versionRenderer->render() << "\n";
        return 0;
    }
    return 1;
}

/**
 * Handle a ParseFailure (error message + help suggestion).
 *
 * The parser writes what the user typed into its message, so the message
 * reaches the terminal as text rather than as instructions.
 */
int Application::handleFailure(const ParseFailure& failure)
{
    std::cerr << printableText(failure.message) << "\n";
    std::cerr << "Run 'scrap --help' for usage information.\n";
    return 1;
}

}  // namespace scrap::Command
