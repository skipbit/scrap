#pragma once

#include "shared/presentation/Presenter.h"
#include <sstream>
#include <string>
#include <vector>

namespace scrap::test {

/**
 * @brief Test implementation of Presenter interface for capturing output
 *
 * This presenter captures all output and errors for testing purposes,
 * allowing tests to verify that operations produce expected output.
 */
class TestPresenter : public scrap::Presenter {
private:
    std::ostringstream output_;
    std::ostringstream errorOutput_;
    std::vector<std::string> messages_;
    std::vector<std::string> errors_;
    scrap::VerbosityLevel verbosityLevel_ = scrap::VerbosityLevel::Normal;
    scrap::OutputFormat outputFormat_ = scrap::OutputFormat::Plain;
    bool colorEnabled_ = false;
    scrap::ProgressStyle progressStyle_ = scrap::ProgressStyle::None;

public:
    TestPresenter() = default;

    // Configuration
    void setOutputFormat(OutputFormat format) override
    {
        outputFormat_ = format;
    }
    void setVerbosityLevel(VerbosityLevel level) override
    {
        verbosityLevel_ = level;
    }
    void setColorEnabled(bool enabled) override
    {
        colorEnabled_ = enabled;
    }
    void setProgressStyle(ProgressStyle style) override
    {
        progressStyle_ = style;
    }

    // Basic output methods
    void displayInfo(const std::string& message) override;
    void displaySuccess(const std::string& message) override;
    void displayWarning(const std::string& message) override;
    void displayError(const std::string& message) override;
    void displayDebug(const std::string& message) override;

    // Progress indicators
    void startProgress(const std::string& task, size_t total) override;
    void updateProgress(size_t current) override;
    void updateProgress(size_t current, const std::string& currentItem) override;
    void finishProgress() override;

    // Structured output
    void displayTable(const Table& table) override;
    void displayTree(const Tree& tree) override;
    void displayList(const std::string& title, const std::vector<std::string>& items) override;

    // Test-specific methods
    [[nodiscard]] std::string output() const
    {
        return output_.str();
    }
    [[nodiscard]] std::string errorOutput() const
    {
        return errorOutput_.str();
    }
    [[nodiscard]] const std::vector<std::string>& messages() const noexcept
    {
        return messages_;
    }
    [[nodiscard]] const std::vector<std::string>& errors() const noexcept
    {
        return errors_;
    }

    void clear();
    [[nodiscard]] bool hasOutput() const
    {
        return ! output_.str().empty();
    }
    [[nodiscard]] bool hasErrors() const noexcept
    {
        return ! errors_.empty();
    }
    [[nodiscard]] std::string lastMessage() const;
    [[nodiscard]] std::string lastError() const;
};

}  // namespace scrap::test
