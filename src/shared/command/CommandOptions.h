#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace scrap {

/**
 * @brief Enumeration for command option types
 */
enum class OptionType {
    String,
    Integer,
    Boolean,
    Flag
};

/**
 * @brief Value object representing a single command option
 *
 * This class follows the builder pattern to allow fluent configuration
 * of option properties while maintaining immutability after construction.
 * Uses PIMPL pattern to ensure ABI safety.
 */
class CommandOption {
public:
    CommandOption(const std::string& name, const std::string& description, OptionType type = OptionType::String);
    ~CommandOption();

    // Copy constructor and assignment
    CommandOption(const CommandOption& other);
    CommandOption& operator=(const CommandOption& other);

    // Move constructor and assignment
    CommandOption(CommandOption&& other) noexcept;
    CommandOption& operator=(CommandOption&& other) noexcept;

    // Accessors (no 'get' prefix per coding standards)
    const std::string& name() const;
    const std::string& description() const;
    OptionType type() const;
    const std::optional<std::string>& defaultValue() const;
    bool required() const;
    const std::vector<std::string>& choices() const;
    const std::string& shortName() const;

    // Builder methods for fluent configuration
    CommandOption& withDefault(const std::string& value);
    CommandOption& withRequired(bool required = true);
    CommandOption& withChoices(const std::vector<std::string>& choices);
    CommandOption& withShortName(const std::string& shortName);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Aggregate containing all command options
 *
 * This class provides a fluent interface for building command option
 * specifications without coupling to any specific CLI library.
 * Uses PIMPL pattern to ensure ABI safety.
 */
class CommandOptions {
public:
    CommandOptions();
    ~CommandOptions();

    // Copy constructor and assignment
    CommandOptions(const CommandOptions& other);
    CommandOptions& operator=(const CommandOptions& other);

    // Move constructor and assignment
    CommandOptions(CommandOptions&& other) noexcept;
    CommandOptions& operator=(CommandOptions&& other) noexcept;

    // Builder methods for adding different option types
    CommandOptions& addPositional(const std::string& name, const std::string& description);
    CommandOptions& addOption(const CommandOption& option);
    CommandOptions& addFlag(const std::string& name, const std::string& description);
    CommandOptions& addFlag(const std::string& name, const std::string& shortName, const std::string& description);

    // Accessors for configured options
    const std::vector<CommandOption>& positionals() const;
    const std::vector<CommandOption>& options() const;
    const std::vector<CommandOption>& flags() const;

    // Check if options are defined
    bool hasOptions() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace scrap
