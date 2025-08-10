#pragma once

#include "shared/command/Operation.h"

namespace scrap {

class Application : public Operation {
public:
    Application();
    virtual ~Application();

    void setup(Command&) override;
    void execute(const std::vector<Command::Option>&) override;

private:
};

}
