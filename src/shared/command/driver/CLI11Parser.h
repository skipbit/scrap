#pragma once

#include "shared/command/CLIParser.h"
#include <memory>

namespace scrap {

// Forward declaration
class Presenter;

/**
 * @brief CLI11-based parser implementation
 *
 * This class provides a concrete implementation of CLIParser using CLI11.
 * Implementation details are completely hidden using PIMPL pattern.
 */
class CLI11Parser : public CLIParser {
public:
    CLI11Parser(const std::string& appName, const std::string& appDescription);
    ~CLI11Parser() override;

    // Non-copyable due to PIMPL
    CLI11Parser(const CLI11Parser&) = delete;
    CLI11Parser& operator=(const CLI11Parser&) = delete;

    // Movable
    CLI11Parser(CLI11Parser&&) noexcept;
    CLI11Parser& operator=(CLI11Parser&&) noexcept;

    CommandRequest parse(int argc, const char* const argv[]) override;
    void configureCommands(const std::vector<std::pair<std::string, std::string>>& commands) override;
    void configureSubcommands(const std::string& parentCommand,
                              const std::vector<std::pair<std::string, std::string>>& subcommands) override;
    std::string helpText(const std::string& commandPath = "") override;
    void configureCommandOptions(const std::string& command,
                                const CommandOptions& options) override;

    /**
     * @brief Set the presenter for custom help formatting
     * @param presenter The presenter to use for help output
     */
    void setPresenter(std::shared_ptr<Presenter> presenter);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Factory for creating CLI11 parsers
 */
class CLI11ParserFactory : public CLIParserFactory {
public:
    std::unique_ptr<CLIParser> createParser(const std::string& appName,
                                            const std::string& appDescription) override;
};

}
