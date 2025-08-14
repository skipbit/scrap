#pragma once

#include <CLI/CLI.hpp>
#include <memory>
#include <string>

namespace scrap {

// Forward declaration
class Presenter;

/**
 * @brief Custom CLI11 formatter that outputs through Presenter
 *
 * This formatter bridges CLI11's help generation with our Presenter abstraction,
 * ensuring consistent and beautiful output across the application.
 */
class PresenterFormatter : public CLI::Formatter {
public:
    /**
     * @brief Construct a new PresenterFormatter
     * @param presenter The presenter to use for output
     */
    explicit PresenterFormatter(std::shared_ptr<Presenter> presenter);

    /**
     * @brief Generate help text for the application
     *
     * This overrides CLI11's default help generation to provide
     * a more beautiful, Presenter-based output.
     */
    std::string make_help(const CLI::App *app, std::string name,
                         CLI::AppFormatMode mode) const override;

private:
    std::shared_ptr<Presenter> presenter_;

    /**
     * @brief Format the usage line
     */
    std::string formatUsage(const CLI::App *app, const std::string& name) const;

    /**
     * @brief Format positional arguments section
     */
    std::string formatPositionals(const CLI::App *app) const;

    /**
     * @brief Format options section
     */
    std::string formatOptions(const CLI::App *app) const;

    /**
     * @brief Format subcommands section
     */
    std::string formatSubcommands(const CLI::App *app) const;
};

} // namespace scrap
