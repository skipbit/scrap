#include "application.h"

int main(const int argc, const char* const argv[]) {

    auto app = scrap::make_command<scrap::application>();
    app.execute(argc, argv);

    return 0;
}
