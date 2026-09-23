#pragma once

#include "command/CommandEntry.h"
#include "command/RuntimeEnvironment.h"

#include <vector>

namespace scrap::Command {

class CommandResolver {
public:
    virtual ~CommandResolver();
    CommandResolver(const CommandResolver&) = default;
    CommandResolver& operator=(const CommandResolver&) = default;
    CommandResolver(CommandResolver&&) = default;
    CommandResolver& operator=(CommandResolver&&) = default;

    virtual std::vector<CommandEntry> resolve(const RuntimeEnvironment& env) = 0;

protected:
    CommandResolver() = default;
};

}  // namespace scrap::Command
