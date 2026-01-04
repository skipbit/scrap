#include "shared/command/Application.h"
#include <exception>
#include <iostream>
#include <span>

int main(const int argc, const char* const argv[])
{
    try {
        scrap::Application app;
        return app.run(std::span<const char* const>{argv, static_cast<size_t>(argc)});
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
