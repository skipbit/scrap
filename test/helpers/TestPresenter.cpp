#include "TestPresenter.h"

namespace scrap::test {

void TestPresenter::displayInfo(const std::string& message)
{
    output_ << "INFO: " << message << "\n";
    messages_.push_back(message);
}

void TestPresenter::displaySuccess(const std::string& message)
{
    output_ << "SUCCESS: " << message << "\n";
    messages_.push_back(message);
}

void TestPresenter::displayWarning(const std::string& message)
{
    output_ << "WARNING: " << message << "\n";
    messages_.push_back(message);
}

void TestPresenter::displayError(const std::string& message)
{
    errorOutput_ << "ERROR: " << message << "\n";
    errors_.push_back(message);
}

void TestPresenter::displayDebug(const std::string& message)
{
    if (verbosityLevel_ >= scrap::VerbosityLevel::Debug) {
        output_ << "DEBUG: " << message << "\n";
        messages_.push_back(message);
    }
}

void TestPresenter::startProgress(const std::string& task, size_t total)
{
    output_ << "Starting: " << task << " (total: " << total << ")\n";
}

void TestPresenter::updateProgress(size_t current)
{
    output_ << "Progress: " << current << "\n";
}

void TestPresenter::updateProgress(size_t current, const std::string& currentItem)
{
    output_ << "Progress: " << current << " (" << currentItem << ")\n";
}

void TestPresenter::finishProgress()
{
    output_ << "Progress: completed\n";
}

void TestPresenter::displayTable(const Table& table)
{
    output_ << "Table:\n";
    for (const auto& header : table.headers) {
        output_ << header << "\t";
    }
    output_ << "\n";
    for (const auto& row : table.rows) {
        for (const auto& cell : row) {
            output_ << cell << "\t";
        }
        output_ << "\n";
    }
}

void TestPresenter::displayTree(const Tree& tree)
{
    output_ << "Tree: " << tree.root->label << "\n";
}

void TestPresenter::displayList(const std::string& title, const std::vector<std::string>& items)
{
    output_ << title << ":\n";
    for (const auto& item : items) {
        output_ << "  - " << item << "\n";
    }
}

void TestPresenter::clear()
{
    output_.str("");
    output_.clear();
    errorOutput_.str("");
    errorOutput_.clear();
    messages_.clear();
    errors_.clear();
}

std::string TestPresenter::lastMessage() const
{
    return messages_.empty() ? "" : messages_.back();
}

std::string TestPresenter::lastError() const
{
    return errors_.empty() ? "" : errors_.back();
}

}  // namespace scrap::test
