#include "shared/command/Application.h"
#include <exception>
#include <iostream>

int main(const int argc, const char* const argv[]) {
    try {
        scrap::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
