#pragma once

#include "command.h"

namespace scrap {

class operation {
public:
    operation();
    operation(const operation&);
    virtual ~operation();

    virtual void setup(command&);
    virtual void execute(const std::vector<command::option>&);

private:
};

}
