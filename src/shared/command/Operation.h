#pragma once

#include "shared/command/Command.h"
#include <vector>

namespace scrap {

class Operation {
public:
    Operation();
    Operation(const Operation&);
    virtual ~Operation();

    virtual void setup(Command&);
    virtual void execute(const std::vector<Command::Option>&);

private:
};

}
