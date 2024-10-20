#include "toolchain.h"
#include "list.h"

#include <iostream>

namespace scrap {

toolchain::toolchain()
{
}

toolchain::~toolchain()
{
}

void toolchain::setup(command& cmd)
{
    cmd.add("list", make_command<list>());
}

void toolchain::execute(const std::vector<command::option>&)
{
    std::cerr << "toolchain help implementation" << std::endl;
}

}
