#pragma once

#include "Operation.h"
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace scrap {

/**
 * @brief Composite operation for handling subcommands
 *
 * This class implements the Composite pattern for operations that
 * have subcommands. It manages a collection of sub-operations and
 * dispatches execution to the appropriate one based on the arguments.
 */
class CompositeOperation : public Operation {
public:
    CompositeOperation();
    virtual ~CompositeOperation();

    /**
     * @brief Add a sub-operation with the given name
     * @param name Name of the subcommand
     * @param operation Operation to execute for this subcommand
     */
    void addSubOperation(const std::string& name, std::shared_ptr<Operation> operation);

    /**
     * @brief Remove a sub-operation
     * @param name Name of the subcommand to remove
     */
    void removeSubOperation(const std::string& name);

    /**
     * @brief Check if a subcommand exists
     * @param name Name of the subcommand
     * @return true if the subcommand exists
     */
    bool hasSubOperation(const std::string& name) const;

    /**
     * @brief Get all subcommand names
     * @return Vector of subcommand names
     */
    std::vector<std::string> subOperationNames() const;

    /**
     * @brief Execute the appropriate sub-operation based on arguments
     * @param args Command line arguments (first should be subcommand name)
     */
    void execute(const std::vector<std::string>& args) override;

    /**
     * @brief Set presenter for this operation and all sub-operations
     * @param presenter Presenter instance
     */
    void setPresenter(std::shared_ptr<Presenter> presenter) override;

protected:
    /**
     * @brief Get a sub-operation by name
     * @param name Name of the subcommand
     * @return Shared pointer to the operation, or nullptr if not found
     */
    std::shared_ptr<Operation> subOperation(const std::string& name) const;

    /**
     * @brief Display help for available subcommands
     */
    virtual void displayHelp() const;

private:
    std::map<std::string, std::shared_ptr<Operation>> subOperations_;
};

} // namespace scrap
