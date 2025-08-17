#pragma once

#include <memory>
#include <string>
#include <vector>

namespace scrap {

// Forward declarations
class Presenter;
class CommandOptions;
class ParsedOptions;

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
    Operation& operator=(const Operation&);
    Operation(Operation&&) noexcept;
    Operation& operator=(Operation&&) noexcept;
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

    /**
     * @brief Describe command options for CLI configuration
     * @return CommandOptions describing this operation's CLI options
     */
    [[nodiscard]] virtual CommandOptions describeOptions() const;

    /**
     * @brief Execute the operation with parsed options
     * @param options Parsed command-line options
     *
     * This method provides a type-safe alternative to string-based argument parsing.
     * The default implementation calls the legacy execute() method for backward compatibility.
     */
    virtual void execute(const ParsedOptions& options);

protected:
    /**
     * @brief Get the current presenter instance
     * @return Shared pointer to presenter
     */
    [[nodiscard]] std::shared_ptr<Presenter> presenter() const;

private:
    std::shared_ptr<Presenter> presenter_;
};

}  // namespace scrap
