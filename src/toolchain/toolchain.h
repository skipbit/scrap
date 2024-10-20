#pragma once

#include "operation.h"

namespace scrap {

class toolchain : public operation {
public:
    toolchain();
    ~toolchain() override;

    void setup(command&) override;
    void execute(const std::vector<command::option>&) override;
};

}
