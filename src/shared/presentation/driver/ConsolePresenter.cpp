#include "shared/presentation/driver/ConsolePresenter.h"
#include <iostream>

namespace scrap {

/**
 * @brief Private implementation class for ConsolePresenter
 */
class ConsolePresenter::Impl {
public:
    void showInfoInternal(const std::string& message)
    {
        std::cout << message << std::endl;
    }
    
    void showErrorInternal(const std::string& message)
    {
        std::cerr << "Error: " << message << std::endl;
    }
    
    void showHelpInternal(const std::string& helpText)
    {
        std::cout << helpText << std::endl;
    }
    
    void showListInternal(const std::string& title, const std::vector<std::string>& items)
    {
        if (!title.empty()) {
            std::cout << title << ":" << std::endl;
        }
        
        for (const auto& item : items) {
            std::cout << "  " << item << std::endl;
        }
        
        if (items.empty()) {
            std::cout << "  (none)" << std::endl;
        }
    }
};

// ConsolePresenter implementation
ConsolePresenter::ConsolePresenter()
    : impl_(std::make_unique<Impl>())
{
}

ConsolePresenter::~ConsolePresenter() = default;

ConsolePresenter::ConsolePresenter(ConsolePresenter&&) noexcept = default;

ConsolePresenter& ConsolePresenter::operator=(ConsolePresenter&&) noexcept = default;

void ConsolePresenter::showInfo(const std::string& message)
{
    impl_->showInfoInternal(message);
}

void ConsolePresenter::showError(const std::string& message)
{
    impl_->showErrorInternal(message);
}

void ConsolePresenter::showHelp(const std::string& helpText)
{
    impl_->showHelpInternal(helpText);
}

void ConsolePresenter::showList(const std::string& title, const std::vector<std::string>& items)
{
    impl_->showListInternal(title, items);
}

// ConsolePresenterFactory implementation
std::unique_ptr<Presenter> ConsolePresenterFactory::createPresenter()
{
    return std::make_unique<ConsolePresenter>();
}

}