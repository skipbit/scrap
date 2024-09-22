#pragma once

#include "command.h"
#include <map>
#include <string>

namespace scrap {

class application : public command {
public:
    application();
    virtual ~application();

    void add(const std::string& key, const scrap::command& command);

    void execute(const int argc, const char* const argv[]);

private:
    std::map<std::string, scrap::command> _commands;
};

}
