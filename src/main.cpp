#include "shared/command/Application.h"

int main(const int argc, const char* const argv[]) {

    auto app = scrap::makeCommand<scrap::Application>();
    app.execute(argc, argv);

    return 0;
}
