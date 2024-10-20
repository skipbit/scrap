#pragma once

#include "operation.h"

namespace scrap {

class application : public operation {
public:
    application();
    virtual ~application();

    void setup(command&) override;
    void execute(const std::vector<command::option>&) override;

private:
};

}
