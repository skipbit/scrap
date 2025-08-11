#pragma once

#include "shared/command/CommandDispatcher.h"
#include <memory>
#include <string>

namespace scrap {

class Operation;

/**
 * @brief CLI11-based command dispatcher implementation
 *
 * This class provides a concrete implementation of CommandDispatcher
 * using CLI11 library. Implementation details are hidden using PIMPL pattern.
 */
class CLI11CommandDispatcher : public CommandDispatcher {
public:
    CLI11CommandDispatcher();
    ~CLI11CommandDispatcher() override;

    // Non-copyable due to PIMPL
    CLI11CommandDispatcher(const CLI11CommandDispatcher&) = delete;
    CLI11CommandDispatcher& operator=(const CLI11CommandDispatcher&) = delete;

    // Movable
    CLI11CommandDispatcher(CLI11CommandDispatcher&&) noexcept;
    CLI11CommandDispatcher& operator=(CLI11CommandDispatcher&&) noexcept;

    CommandResult dispatch(const CommandRequest& request) override;
    void registerOperation(const std::string& commandName,
                           std::shared_ptr<Operation> operation) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
