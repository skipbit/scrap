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