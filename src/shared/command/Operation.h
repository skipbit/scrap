#pragma once

#include <vector>
#include <string>
#include <memory>

namespace scrap {

// Forward declaration
class Presenter;

/**
 * @brief Base class for all executable operations
 *
 * This class provides the interface for domain operations in the Clean Architecture.
 * It has been simplified to remove dependencies on infrastructure details.
 */
class Operation {
public:
    Operation();
    Operation(const Operation&);
    virtual ~Operation();

    /**
     * @brief Set the presenter for output handling
     * @param presenter Presenter instance for output formatting
     */
    virtual void setPresenter(std::shared_ptr<Presenter> presenter);

    /**
     * @brief Execute the operation with provided arguments
     * @param args Command line arguments
     */
    virtual void execute(const std::vector<std::string>& args);

protected:
    /**
     * @brief Get the current presenter instance
     * @return Shared pointer to presenter
     */
    std::shared_ptr<Presenter> getPresenter() const;

private:
    std::shared_ptr<Presenter> presenter_;
};

}
