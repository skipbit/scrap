#include "list.h"

#include <dross/platform/xdg.h>
#include <iostream>

namespace scrap {

list::list()
{
}

list::~list()
{
}

void list::execute(const std::vector<command::option>&)
{
    const auto directory = dross::xdg("scrap").data_home();
    std::cout << "list operation executed: " << directory.value() << std::endl;
}

}
