#include "application.h"
#include "toolchain/toolchain.h"

int main(const int argc, const char* const argv[]) {

    scrap::application app;
    app.add("toolchain", scrap::toolchain());
    app.execute(argc, argv);

    return 0;
}
