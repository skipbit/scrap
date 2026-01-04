#pragma once

#include "shared/command/ApplicationCommandHandler.h"
#include <memory>
#include <span>

namespace scrap {

/**
 * @brief Main application entry point following Clean Architecture
 *
 * This class represents the main application orchestrator that
 * coordinates CLI parsing and command execution without exposing
 * implementation details.
 */
class Application {
public:
    Application();
    ~Application();

    // Non-copyable due to unique_ptr member
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Movable
    Application(Application&&) noexcept;
    Application& operator=(Application&&) noexcept;

    /**
     * @brief Run the application
     * @param args Command line arguments as a span
     * @return Exit code
     */
    int run(std::span<const char* const> args);

private:
    std::unique_ptr<ApplicationCommandHandler> commandHandler_;

    void registerOperations();
};

}
