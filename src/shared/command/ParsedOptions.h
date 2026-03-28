#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace scrap {

/**
 * @brief Container for parsed command-line options
 *
 * This class provides type-safe access to parsed command-line options
 * without exposing the underlying CLI library implementation.
 * Uses PIMPL pattern to ensure ABI safety.
 */
class ParsedOptions {
public:
    using Value = std::variant<std::string, int, bool, std::vector<std::string>>;

    ParsedOptions();
    ~ParsedOptions();

    // Copy constructor and assignment
    ParsedOptions(const ParsedOptions& other);
    ParsedOptions& operator=(const ParsedOptions& other);

    // Move constructor and assignment
    ParsedOptions(ParsedOptions&& other) noexcept;
    ParsedOptions& operator=(ParsedOptions&& other) noexcept;

    /**
     * @brief Set a parsed option value
     * @param key Option name
     * @param value Parsed value
     */
    void set(const std::string& key, const Value& value);

    /**
     * @brief Get a string option value
     * @param key Option name
     * @return Optional containing the value if present and is a string
     */
    std::optional<std::string> string(const std::string& key) const;

    /**
     * @brief Get an integer option value
     * @param key Option name
     * @return Optional containing the value if present and is an integer
     */
    std::optional<int> integer(const std::string& key) const;

    /**
     * @brief Check if a flag is set
     * @param key Flag name
     * @return True if flag is present and set
     */
    bool flag(const std::string& key) const;

    /**
     * @brief Get a list of string values (for repeated options)
     * @param key Option name
     * @return Vector of strings, empty if not present
     */
    std::vector<std::string> stringList(const std::string& key) const;

    /**
     * @brief Check if an option is present
     * @param key Option name
     * @return True if option was provided
     */
    bool has(const std::string& key) const;

    /**
     * @brief Get all remaining positional arguments
     * @return Vector of positional arguments
     */
    const std::vector<std::string>& positionalArgs() const;

    /**
     * @brief Set positional arguments
     * @param args Positional arguments
     */
    void setPositionalArgs(const std::vector<std::string>& args);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace scrap
