#pragma once

#include <memory>
#include <string>
#include <vector>

namespace scrap {

// Forward declarations
struct Table;
struct Tree;

/**
 * @brief Output format options
 */
enum class OutputFormat {
    Plain,
    Json,
    Xml,
    Table
};

/**
 * @brief Verbosity level for output
 */
enum class VerbosityLevel {
    Quiet = 0,
    Normal = 1,
    Verbose = 2,
    Debug = 3
};

/**
 * @brief Progress indicator style
 */
enum class ProgressStyle {
    None,
    Simple,
    Rich
};

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

    // Configuration
    virtual void setOutputFormat(OutputFormat format) = 0;
    virtual void setVerbosityLevel(VerbosityLevel level) = 0;
    virtual void setColorEnabled(bool enabled) = 0;
    virtual void setProgressStyle(ProgressStyle style) = 0;

    // Basic output methods
    virtual void displayInfo(const std::string& message) = 0;
    virtual void displaySuccess(const std::string& message) = 0;
    virtual void displayWarning(const std::string& message) = 0;
    virtual void displayError(const std::string& message) = 0;
    virtual void displayDebug(const std::string& message) = 0;

    // Progress indicators
    virtual void startProgress(const std::string& task, size_t total) = 0;
    virtual void updateProgress(size_t current) = 0;
    virtual void updateProgress(size_t current, const std::string& currentItem) = 0;
    virtual void finishProgress() = 0;

    // Structured output
    virtual void displayTable(const Table& table) = 0;
    virtual void displayTree(const Tree& tree) = 0;
    virtual void displayList(const std::string& title, const std::vector<std::string>& items) = 0;
};

/**
 * @brief Table structure for tabular output
 */
struct Table {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

/**
 * @brief Tree structure for hierarchical output
 */
struct Tree {
    struct Node {
        std::string label;
        std::vector<std::unique_ptr<Node>> children;
    };
    std::unique_ptr<Node> root;
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

}  // namespace scrap
