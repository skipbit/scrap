#include "command/Application.h"
#include "command/BuiltinCommandResolver.h"
#include "command/DefaultHelpRenderer.h"
#include "command/DefaultVersionRenderer.h"
#include "command/ExternalCommandResolver.h"
#include "command/HelpRenderer.h"
#include "command/MetadataProtocolProvider.h"
#include "command/ProjectCommandResolver.h"
#include "command/RuntimeEnvironment.h"
#include "command/RuntimeEnvironmentFactory.h"
#include "command/StubScriptsReader.h"
#include "command/VersionRenderer.h"
#include "command/driver/CLI11ParserAdapter.h"

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <system_error>

using namespace scrap::Command;

/**
 * Composition root: wires the CLI parser, renderers, and command resolvers
 * together and dispatches to Application::run().
 */
int main(int argc, char* argv[])
{
    try {
        const char* scrapHomeEnv = std::getenv("SCRAP_HOME");
        const char* pathEnv = std::getenv("PATH");

        std::error_code ec;
        auto cwd = std::filesystem::current_path(ec);
        if (ec) {
            cwd = ".";
        }

        auto env = makeRuntimeEnvironment(cwd,
                                          scrapHomeEnv != nullptr ? std::string{scrapHomeEnv} : std::string{},
                                          pathEnv != nullptr ? std::string{pathEnv} : std::string{});

        auto helpRenderer = std::make_unique<DefaultHelpRenderer>();
        auto versionRenderer = std::make_unique<DefaultVersionRenderer>();
        HelpRenderer& helpRef = *helpRenderer;
        VersionRenderer& versionRef = *versionRenderer;

        Application app(std::make_unique<CLI11ParserAdapter>(), std::move(helpRenderer), std::move(versionRenderer));
        app.addResolver(std::make_unique<BuiltinCommandResolver>(helpRef, versionRef));
        app.addResolver(std::make_unique<ExternalCommandResolver>(std::make_unique<MetadataProtocolProvider>()));
        app.addResolver(std::make_unique<ProjectCommandResolver>(std::make_unique<StubScriptsReader>()));

        return app.run(std::span<const char* const>{argv, static_cast<size_t>(argc)}, env);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
