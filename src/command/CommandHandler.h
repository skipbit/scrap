#pragma once

namespace scrap::Command {

struct InvocationContext;

class CommandHandler {
public:
    virtual ~CommandHandler();
    virtual auto execute(const InvocationContext& ctx) -> int = 0;
};

}  // namespace scrap::Command
