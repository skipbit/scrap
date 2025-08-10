#include "shared/command/Operation.h"

namespace scrap {

Operation::Operation() = default;

Operation::Operation(const Operation&) = default;

Operation::~Operation() = default;

void Operation::setup(Command&)
{
}

void Operation::execute(const std::vector<Command::Option>&)
{
}

}