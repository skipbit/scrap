#include "operation.h"

#include <iostream>

namespace scrap {

operation::operation() = default;

operation::operation(const operation&) = default;

operation::~operation() = default;

void operation::execute()
{
    std::cout << "Not yet implemented" << std::endl;
}

}
