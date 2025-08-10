#pragma once

#include <vector>
#include <string>

namespace scrap {

class Command;

class Operation {
public:
    Operation();
    Operation(const Operation&);
    virtual ~Operation();

    virtual void setup(Command&);

    // Simple execute method with command line arguments
    virtual void execute(const std::vector<std::string>& args);

private:
};

}
