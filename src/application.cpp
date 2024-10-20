#include "application.h"
#include "toolchain/toolchain.h"

#include <iostream>

namespace scrap {

application::application() = default;

application::~application() = default;

void application::setup(command& cmd)
{
    cmd.add("toolchain", make_command<scrap::toolchain>());
}

void application::execute(const std::vector<command::option>&)
{
    std::cerr << "application help implementation" << std::endl;
}

}
