#pragma once

#include "command/CommandEntry.h"
#include "command/RuntimeEnvironment.h"

#include <vector>

namespace scrap::Command {

class CommandResolver {
public:
    virtual ~CommandResolver();
    virtual auto resolve(const RuntimeEnvironment& env) -> std::vector<CommandEntry> = 0;
};

}  // namespace scrap::Command
