#pragma once

namespace scrap::Command {

struct InvocationContext;

class CommandHandler {
public:
    virtual ~CommandHandler();
    CommandHandler(const CommandHandler&) = default;
    CommandHandler& operator=(const CommandHandler&) = default;
    CommandHandler(CommandHandler&&) = default;
    CommandHandler& operator=(CommandHandler&&) = default;

    virtual int execute(const InvocationContext& ctx) = 0;

protected:
    CommandHandler() = default;
};

}  // namespace scrap::Command
