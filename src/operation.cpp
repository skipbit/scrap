#include "operation.h"

#include <iostream>

namespace scrap {

operation::operation() = default;

operation::operation(const operation&) = default;

operation::~operation() = default;

void operation::setup(command&)
{
}

void operation::execute(const std::vector<command::option>&)
{
    std::cout << "Not yet implemented" << std::endl;
}

}
