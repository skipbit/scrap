#pragma once

#include <string>
#include <vector>
#include <memory>

namespace scrap {

/**
 * @brief Abstract interface for output presentation
 * 
 * This interface separates business logic from output formatting,
 * allowing different output formats (console, JSON, XML) without
 * affecting the domain layer.
 */
class Presenter {
public:
    virtual ~Presenter() = default;
    
    /**
     * @brief Display informational message
     * @param message Information to display
     */
    virtual void showInfo(const std::string& message) = 0;
    
    /**
     * @brief Display error message
     * @param message Error information to display
     */
    virtual void showError(const std::string& message) = 0;
    
    /**
     * @brief Display help text
     * @param helpText Help content to display
     */
    virtual void showHelp(const std::string& helpText) = 0;
    
    /**
     * @brief Display list of items
     * @param title List title
     * @param items List of items to display
     */
    virtual void showList(const std::string& title, const std::vector<std::string>& items) = 0;
};

/**
 * @brief Factory for creating presenters
 */
class PresenterFactory {
public:
    virtual ~PresenterFactory() = default;
    
    /**
     * @brief Create a presenter instance
     * @return Unique pointer to presenter
     */
    virtual std::unique_ptr<Presenter> createPresenter() = 0;
};

}