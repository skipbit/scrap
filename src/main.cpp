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
#include "project/driver/DiskProjectFileSystem.h"

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

namespace {

/**
 * The value of the environment variable @p name, empty when it is unset.
 */
std::string environmentVariable(const char* name)
{
    const char* value = std::getenv(name);
    return (value != nullptr) ? std::string{ value } : std::string{};
}

}  // namespace

/**
 * Composition root: wires the CLI parser, renderers, and command resolvers
 * together and dispatches to Application::run().
 */
int main(int argc, char* argv[])
{
    try {
        std::error_code ec;
        auto cwd = std::filesystem::current_path(ec);
        if (ec) {
            cwd = ".";
        }

        auto env = makeRuntimeEnvironment(ProcessInputs{
            .workingDirectory = cwd,
            .scrapHome = environmentVariable("SCRAP_HOME"),
            .home = environmentVariable("HOME"),
            .path = environmentVariable("PATH"),
            .compiler = environmentVariable("CXX"),
            .toolchain = environmentVariable("SCRAP_TOOLCHAIN"),
            .toolchainIndex = environmentVariable("SCRAP_TOOLCHAIN_INDEX"),
            .certificateFile = environmentVariable("SSL_CERT_FILE"),
        });

        auto helpRenderer = std::make_unique<DefaultHelpRenderer>();
        auto versionRenderer = std::make_unique<DefaultVersionRenderer>();
        HelpRenderer& helpRef = *helpRenderer;
        VersionRenderer& versionRef = *versionRenderer;

        scrap::Project::DiskProjectFileSystem fileSystem;

        Application app(std::make_unique<CLI11ParserAdapter>(), std::move(helpRenderer), std::move(versionRenderer));
        app.addResolver(std::make_unique<BuiltinCommandResolver>(helpRef, versionRef, fileSystem));
        app.addResolver(std::make_unique<ExternalCommandResolver>(std::make_unique<MetadataProtocolProvider>()));
        app.addResolver(std::make_unique<ProjectCommandResolver>(std::make_unique<StubScriptsReader>()));

        return app.run(std::span<const char* const>{ argv, static_cast<size_t>(argc) }, env);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
