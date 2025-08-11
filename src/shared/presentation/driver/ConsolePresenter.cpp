#include "shared/presentation/driver/ConsolePresenter.h"
#include <iostream>
#include <iomanip>

namespace scrap {

/**
 * @brief Private implementation class for ConsolePresenter
 */
class ConsolePresenter::Impl {
public:
    Impl() : outputFormat_(OutputFormat::Plain), verbosity_(VerbosityLevel::Normal),
             useColor_(true), progressStyle_(ProgressStyle::Simple),
             progressTotal_(0), progressCurrent_(0) {}

    // Configuration
    OutputFormat outputFormat_;
    VerbosityLevel verbosity_;
    bool useColor_;
    ProgressStyle progressStyle_;

    // Progress tracking
    std::string progressTask_;
    size_t progressTotal_;
    size_t progressCurrent_;

    void showInfoInternal(const std::string& message, const std::string& prefix = "") {
        if (useColor_) {
            std::cout << "\033[32m" << prefix << "\033[0m" << message << std::endl;
        } else {
            std::cout << prefix << message << std::endl;
        }
    }

    void showSuccessInternal(const std::string& message) {
        if (useColor_) {
            std::cout << "\033[32m✓\033[0m " << message << std::endl;
        } else {
            std::cout << "success: " << message << std::endl;
        }
    }

    void showWarningInternal(const std::string& message) {
        if (useColor_) {
            std::cerr << "\033[33m⚠\033[0m " << message << std::endl;
        } else {
            std::cerr << "warning: " << message << std::endl;
        }
    }

    void showErrorInternal(const std::string& message) {
        if (useColor_) {
            std::cerr << "\033[31m✗\033[0m " << message << std::endl;
        } else {
            std::cerr << "error: " << message << std::endl;
        }
    }

    void showDebugInternal(const std::string& message) {
        if (verbosity_ >= VerbosityLevel::Debug) {
            if (useColor_) {
                std::cout << "\033[90m[DEBUG] " << message << "\033[0m" << std::endl;
            } else {
                std::cout << "[DEBUG] " << message << std::endl;
            }
        }
    }

    void showProgressInternal(size_t current) {
        if (progressStyle_ == ProgressStyle::None) {
            return;
        }

        if (progressStyle_ == ProgressStyle::Simple) {
            if (progressTotal_ > 0) {
                double percentage = (static_cast<double>(current) / progressTotal_) * 100.0;
                std::cout << "\r" << std::fixed << std::setprecision(1) << percentage << "%" << std::flush;
            }
        }
    }

    void showListInternal(const std::string& title, const std::vector<std::string>& items) {
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

    void showTableInternal(const Table& table) {
        // Simple table implementation
        if (table.headers.empty() && table.rows.empty()) {
            return;
        }

        // Calculate column widths
        std::vector<size_t> colWidths;
        if (!table.headers.empty()) {
            colWidths.resize(table.headers.size());
            for (size_t i = 0; i < table.headers.size(); ++i) {
                colWidths[i] = table.headers[i].length();
            }
        }

        for (const auto& row : table.rows) {
            if (colWidths.size() < row.size()) {
                colWidths.resize(row.size());
            }
            for (size_t i = 0; i < row.size(); ++i) {
                colWidths[i] = std::max(colWidths[i], row[i].length());
            }
        }

        // Print headers
        if (!table.headers.empty()) {
            for (size_t i = 0; i < table.headers.size(); ++i) {
                if (i > 0) std::cout << " | ";
                std::cout << std::left << std::setw(colWidths[i]) << table.headers[i];
            }
            std::cout << std::endl;

            // Print separator
            for (size_t i = 0; i < table.headers.size(); ++i) {
                if (i > 0) std::cout << "-|-";
                std::cout << std::string(colWidths[i], '-');
            }
            std::cout << std::endl;
        }

        // Print rows
        for (const auto& row : table.rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) std::cout << " | ";
                std::cout << std::left << std::setw(colWidths[i]) << row[i];
            }
            std::cout << std::endl;
        }
    }

    void showTreeInternal(const Tree& tree) {
        if (tree.root) {
            showTreeNodeInternal(tree.root.get(), "");
        }
    }

private:
    void showTreeNodeInternal(const Tree::Node* node, const std::string& prefix) {
        if (!node) return;

        std::cout << prefix << node->label << std::endl;

        for (size_t i = 0; i < node->children.size(); ++i) {
            bool isLast = (i == node->children.size() - 1);
            std::string childPrefix = prefix + (isLast ? "└── " : "├── ");
            std::string nextPrefix = prefix + (isLast ? "    " : "│   ");

            std::cout << childPrefix << node->children[i]->label << std::endl;
            showTreeNodeInternal(node->children[i].get(), nextPrefix);
        }
    }
};

// ConsolePresenter implementation
ConsolePresenter::ConsolePresenter()
    : impl_(std::make_unique<Impl>()) {
}

ConsolePresenter::~ConsolePresenter() = default;

ConsolePresenter::ConsolePresenter(ConsolePresenter&&) noexcept = default;

ConsolePresenter& ConsolePresenter::operator=(ConsolePresenter&&) noexcept = default;

// Configuration methods
void ConsolePresenter::setOutputFormat(OutputFormat format) {
    impl_->outputFormat_ = format;
}

void ConsolePresenter::setVerbosityLevel(VerbosityLevel level) {
    impl_->verbosity_ = level;
}

void ConsolePresenter::setColorEnabled(bool enabled) {
    impl_->useColor_ = enabled;
}

void ConsolePresenter::setProgressStyle(ProgressStyle style) {
    impl_->progressStyle_ = style;
}

// Output methods
void ConsolePresenter::displayInfo(const std::string& message) {
    impl_->showInfoInternal(message);
}

void ConsolePresenter::displaySuccess(const std::string& message) {
    impl_->showSuccessInternal(message);
}

void ConsolePresenter::displayWarning(const std::string& message) {
    impl_->showWarningInternal(message);
}

void ConsolePresenter::displayError(const std::string& message) {
    impl_->showErrorInternal(message);
}

void ConsolePresenter::displayDebug(const std::string& message) {
    impl_->showDebugInternal(message);
}

// Progress methods
void ConsolePresenter::startProgress(const std::string& task, size_t total) {
    impl_->progressTask_ = task;
    impl_->progressTotal_ = total;
    impl_->progressCurrent_ = 0;

    if (impl_->progressStyle_ != ProgressStyle::None) {
        std::cout << task << "..." << std::flush;
    }
}

void ConsolePresenter::updateProgress(size_t current) {
    impl_->progressCurrent_ = current;
    impl_->showProgressInternal(current);
}

void ConsolePresenter::updateProgress(size_t current, const std::string& /*currentItem*/) {
    updateProgress(current);
}

void ConsolePresenter::finishProgress() {
    if (impl_->progressStyle_ != ProgressStyle::None) {
        std::cout << " done" << std::endl;
    }
    impl_->progressTask_.clear();
    impl_->progressTotal_ = 0;
    impl_->progressCurrent_ = 0;
}

// Structured output methods
void ConsolePresenter::displayTable(const Table& table) {
    impl_->showTableInternal(table);
}

void ConsolePresenter::displayTree(const Tree& tree) {
    impl_->showTreeInternal(tree);
}

void ConsolePresenter::displayList(const std::string& title, const std::vector<std::string>& items) {
    impl_->showListInternal(title, items);
}

// Legacy compatibility methods
void ConsolePresenter::showInfo(const std::string& message) {
    displayInfo(message);
}

void ConsolePresenter::showError(const std::string& message) {
    displayError(message);
}

void ConsolePresenter::showHelp(const std::string& helpText) {
    displayInfo(helpText);
}

void ConsolePresenter::showList(const std::string& title, const std::vector<std::string>& items) {
    displayList(title, items);
}

// ConsolePresenterFactory implementation
std::unique_ptr<Presenter> ConsolePresenterFactory::createPresenter() {
    return std::make_unique<ConsolePresenter>();
}

} // namespace scrap