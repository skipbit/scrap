#pragma once

#include "shared/command/Operation.h"

namespace scrap {

class Application : public Operation {
public:
    Application();
    virtual ~Application();

    void setup(Command&) override;
    void execute(const std::vector<std::string>& args) override;

    // Custom run method
    void run(int argc, const char* const argv[]);

private:
};

}
