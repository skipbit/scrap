#pragma once

#include "shared/command/Operation.h"

namespace scrap::toolchain {

class ToolchainCommand : public scrap::Operation {
public:
    ToolchainCommand();
    ~ToolchainCommand() override;

    void setup(Command&) override;
    void execute(const std::vector<std::string>& args) override;
};

}
