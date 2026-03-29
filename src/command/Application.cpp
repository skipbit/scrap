#include "command/Application.h"

#include "command/CommandCatalog.h"
#include "command/CommandResolver.h"
#include "command/HelpRenderer.h"
#include "command/InvocationContext.h"
#include "command/ParseResult.h"
#include "command/ParserAdapter.h"
#include "command/RuntimeEnvironment.h"
#include "command/VersionRenderer.h"

#include <iostream>
#include <memory>
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
    : parser_(std::move(parser)), helpRenderer_(std::move(helpRenderer)), versionRenderer_(std::move(versionRenderer))
{
}

Application::~Application() = default;
Application::Application(Application&&) noexcept = default;
Application& Application::operator=(Application&&) noexcept = default;

/**
 * Register a command resolver for the Resolve phase.
 */
auto Application::addResolver(std::unique_ptr<CommandResolver> resolver) -> void
{
    resolvers_.push_back(std::move(resolver));
}

/**
 * Execute the four-phase CLI pipeline: Resolve, Configure, Parse, Execute.
 */
auto Application::run(std::span<const char* const> argv, const RuntimeEnvironment& env) -> int
{
    // Phase 1: Resolve — collect CommandEntry trees from all resolvers.
    CommandCatalog catalog;
    for (auto& resolver : resolvers_) {
        catalog.addEntries(resolver->resolve(env));
    }

    // Phase 2: Configure — derive CommandSpec tree and feed to parser.
    auto specTree = catalog.specs();
    parser_->configure(specTree);

    // Phase 3: Parse — parse argv into a ParseResult.
    auto result = parser_->parse(argv);

    // Phase 4: Execute — dispatch based on ParseResult.
    if (result.has_value()) {
        auto& invocation = *result;
        const auto* entry = catalog.find(invocation.commandPath);
        if (entry == nullptr) {
            std::cerr << "Internal error: command not found after parsing: " << invocation.commandPath << "\n";
            return 1;
        }
        auto handler = entry->createHandler(invocation.options);
        const InvocationContext ctx{invocation.options, &env, &catalog};
        return handler->execute(ctx);
    }

    return std::visit(
        [&](const auto& value) -> int {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ParseDirective>) {
                return handleDirective(catalog, value);
            } else {
                return handleFailure(value);
            }
        },
        result.error());
}

/**
 * Handle a ParseDirective (help or version request).
 */
auto Application::handleDirective(const CommandCatalog& catalog, const ParseDirective& directive) -> int
{
    switch (directive.kind) {
        case ParseDirectiveKind::HelpRequested:
            return handleHelp(catalog, directive.target);
        case ParseDirectiveKind::VersionRequested:
            std::cout << versionRenderer_->render() << "\n";
            return 0;
    }
    return 1;
}

/**
 * Render help for a specific command or the global listing.
 */
auto Application::handleHelp(const CommandCatalog& catalog, const std::optional<std::string>& target) -> int
{
    if (! target.has_value()) {
        std::cout << helpRenderer_->renderGlobal(catalog.helpEntries());
        return 0;
    }
    for (const auto& spec : catalog.specs()) {
        if (spec.name == *target) {
            std::cout << helpRenderer_->renderCommand(spec);
            return 0;
        }
    }
    std::cerr << "Unknown command: " << *target << "\n";
    std::cerr << "Run 'scrap --help' for usage information.\n";
    return 1;
}

/**
 * Handle a ParseFailure (error message + help suggestion).
 */
auto Application::handleFailure(const ParseFailure& failure) -> int
{
    std::cerr << failure.message << "\n";
    std::cerr << "Run 'scrap --help' for usage information.\n";
    return 1;
}

}  // namespace scrap::Command
