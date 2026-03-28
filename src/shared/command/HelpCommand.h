#pragma once

#include "shared/command/CLIParser.h"
#include "shared/command/Operation.h"
#include <memory>

namespace scrap {

/**
 * @brief Generic help command that uses CLI parser's help functionality
 *
 * This command displays help information for any command path using
 * the CLI parser's built-in help system.
 */
class HelpCommand : public Operation {
public:
    explicit HelpCommand(std::shared_ptr<CLIParser> parser, const std::string& commandPath = "");
    ~HelpCommand() override;

    void execute(const std::vector<std::string>& args) override;

private:
    std::shared_ptr<CLIParser> parser_;
    std::string commandPath_;
};

}  // namespace scrap
