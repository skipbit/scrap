#include "command/Application.h"

#include "command/CommandCatalog.h"
#include "command/InvocationContext.h"

#include <iostream>
#include <variant>

namespace scrap::Command {

namespace {

/**
 * Visitor helper for std::visit with multiple lambdas.
 */
template <class... Ts> struct Overloaded : Ts... {
    using Ts::operator()...;
};

}  // namespace

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
        if (! entry) {
            std::cerr << "Internal error: command not found after parsing: " << invocation.commandPath << "\n";
            return 1;
        }

        auto handler = entry->createHandler(invocation.options);
        InvocationContext ctx{invocation.options, env, catalog};
        return handler->execute(ctx);
    }

    // Handle interruptions (directives and failures).
    return std::visit(Overloaded{
                          [&](const ParseDirective& directive) -> int {
                              switch (directive.kind) {
                                  case ParseDirectiveKind::HelpRequested: {
                                      if (! directive.target.has_value()) {
                                          std::cout << helpRenderer_->renderGlobal(catalog.helpEntries());
                                      } else {
                                          // Find the matching spec with subcommands populated.
                                          auto specs = catalog.specs();
                                          for (const auto& spec : specs) {
                                              if (spec.name == *directive.target) {
                                                  std::cout << helpRenderer_->renderCommand(spec);
                                                  return 0;
                                              }
                                          }
                                          std::cerr << "Unknown command: " << *directive.target << "\n";
                                          std::cerr << "Run 'scrap --help' for usage information.\n";
                                          return 1;
                                      }
                                      return 0;
                                  }
                                  case ParseDirectiveKind::VersionRequested: {
                                      std::cout << versionRenderer_->render() << "\n";
                                      return 0;
                                  }
                              }
                              return 1;
                          },
                          [&](const ParseFailure& failure) -> int {
                              std::cerr << failure.message << "\n";
                              std::cerr << "Run 'scrap --help' for usage information.\n";
                              return 1;
                          },
                      },
                      result.error());
}

}  // namespace scrap::Command
