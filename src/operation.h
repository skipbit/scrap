#pragma once

#include "command.h"

namespace scrap {

class operation : public scrap::command {
public:
    operation();
    operation(const operation&);
    virtual ~operation();

    virtual void execute();

private:
};

}
