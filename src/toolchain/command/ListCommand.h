#pragma once

#include "shared/command/Operation.h"

namespace scrap::toolchain {

class ListCommand : public scrap::Operation {
public:
    ListCommand();
    ~ListCommand() override;

    void execute(const std::vector<std::string>& args) override;
};

}
