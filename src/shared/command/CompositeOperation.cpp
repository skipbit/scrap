#include "CompositeOperation.h"
#include "shared/presentation/Presenter.h"
#include <algorithm>
#include <sstream>

namespace scrap {

CompositeOperation::CompositeOperation() = default;

CompositeOperation::~CompositeOperation() = default;

void CompositeOperation::addSubOperation(const std::string& name, std::shared_ptr<Operation> operation)
{
    if (operation) {
        subOperations_[name] = operation;
        // Propagate presenter if already set
        if (auto output = presenter()) {
            operation->setPresenter(output);
        }
    }
}

void CompositeOperation::removeSubOperation(const std::string& name)
{
    subOperations_.erase(name);
}

bool CompositeOperation::hasSubOperation(const std::string& name) const
{
    return subOperations_.find(name) != subOperations_.end();
}

std::vector<std::string> CompositeOperation::subOperationNames() const
{
    std::vector<std::string> names;
    names.reserve(subOperations_.size());
    for (const auto& [name, _] : subOperations_) {
        names.push_back(name);
    }
    return names;
}

std::shared_ptr<Operation> CompositeOperation::subOperation(const std::string& name) const
{
    auto it = subOperations_.find(name);
    return (it != subOperations_.end()) ? it->second : nullptr;
}

void CompositeOperation::execute(const std::vector<std::string>& args)
{
    if (args.empty() || args[0] == "help" || args[0] == "--help") {
        displayHelp();
        return;
    }

    const std::string& subcommand = args[0];
    auto operation = subOperation(subcommand);

    if (! operation) {
        auto output = presenter();
        if (output) {
            std::stringstream ss;
            ss << "Unknown subcommand: '" << subcommand << "'";
            output->displayError(ss.str());
            output->displayInfo("Run with 'help' to see available subcommands");
        }
        return;
    }

    // Create new args without the subcommand
    std::vector<std::string> subArgs(args.begin() + 1, args.end());
    operation->execute(subArgs);
}

void CompositeOperation::setPresenter(std::shared_ptr<Presenter> presenter)
{
    Operation::setPresenter(presenter);
    // Propagate to all sub-operations
    for (auto& [_, operation] : subOperations_) {
        if (operation) {
            operation->setPresenter(presenter);
        }
    }
}

void CompositeOperation::displayHelp() const
{
    auto output = presenter();
    if (! output) {
        return;
    }

    output->displayInfo("Available subcommands:");
    auto names = subOperationNames();
    std::sort(names.begin(), names.end());

    for (const auto& name : names) {
        std::stringstream ss;
        ss << "  " << name;
        output->displayInfo(ss.str());
    }
}

}  // namespace scrap
