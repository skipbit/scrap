#include "shared/command/Application.h"
#include "shared/command/Command.h"
#include <iostream>
#include <exception>

int main(const int argc, const char* const argv[]) {
    try {
        // Create Application directly for parser type selection
        scrap::Application app;
        app.run(argc, argv);  // Use Application's custom run with parser selection
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
