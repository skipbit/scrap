#include <iostream>
#include <span>

int main(const int argc, const char* const argv[]) {
    const std::span<const char* const> args(argv, argc);
    for (const auto& i : args) {
        std::cout << i << std::endl;
    }
    return 0;
}
