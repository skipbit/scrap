#pragma once

#include "shared/command/ApplicationCommandHandler.h"
#include <memory>

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
     * @param argc Argument count
     * @param argv Argument vector
     * @return Exit code
     */
    int run(int argc, const char* const argv[]);

private:
    std::unique_ptr<ApplicationCommandHandler> commandHandler_;
    
    void registerOperations();
};

}
