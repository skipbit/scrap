#pragma once

#include "command/ParsedOptions.h"
#include "command/RuntimeEnvironment.h"

namespace scrap::Command {

class CommandCatalog;

struct InvocationContext {
    ParsedOptions options;
    const RuntimeEnvironment& env;
    const CommandCatalog& catalog;
};

}  // namespace scrap::Command
