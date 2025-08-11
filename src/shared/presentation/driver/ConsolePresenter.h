#pragma once

#include "shared/presentation/Presenter.h"
#include <memory>

namespace scrap {

/**
 * @brief Console-based presenter implementation
 *
 * This class provides a concrete implementation of Presenter for console output.
 * Implementation details are hidden using PIMPL pattern.
 */
class ConsolePresenter : public Presenter {
public:
    ConsolePresenter();
    ~ConsolePresenter() override;

    // Non-copyable due to PIMPL
    ConsolePresenter(const ConsolePresenter&) = delete;
    ConsolePresenter& operator=(const ConsolePresenter&) = delete;

    // Movable
    ConsolePresenter(ConsolePresenter&&) noexcept;
    ConsolePresenter& operator=(ConsolePresenter&&) noexcept;

    // Configuration methods
    void setOutputFormat(OutputFormat format) override;
    void setVerbosityLevel(VerbosityLevel level) override;
    void setColorEnabled(bool enabled) override;
    void setProgressStyle(ProgressStyle style) override;

    // Output methods
    void displayInfo(const std::string& message) override;
    void displaySuccess(const std::string& message) override;
    void displayWarning(const std::string& message) override;
    void displayError(const std::string& message) override;
    void displayDebug(const std::string& message) override;

    // Progress methods
    void startProgress(const std::string& task, size_t total) override;
    void updateProgress(size_t current) override;
    void updateProgress(size_t current, const std::string& currentItem) override;
    void finishProgress() override;

    // Structured output methods
    void displayTable(const Table& table) override;
    void displayTree(const Tree& tree) override;
    void displayList(const std::string& title, const std::vector<std::string>& items) override;

    // Legacy compatibility
    void showInfo(const std::string& message) override;
    void showError(const std::string& message) override;
    void showHelp(const std::string& helpText) override;
    void showList(const std::string& title, const std::vector<std::string>& items) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Factory for creating console presenters
 */
class ConsolePresenterFactory : public PresenterFactory {
public:
    std::unique_ptr<Presenter> createPresenter() override;
};

}
