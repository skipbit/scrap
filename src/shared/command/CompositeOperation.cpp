#include "CompositeOperation.h"
#include "shared/presentation/Presenter.h"
#include <algorithm>
#include <sstream>

namespace scrap {

CompositeOperation::CompositeOperation() = default;

CompositeOperation::~CompositeOperation() = default;

void CompositeOperation::addSubOperation(const std::string& name,
                                         std::shared_ptr<Operation> operation) {
    if (operation) {
        subOperations_[name] = operation;
        // Propagate presenter if already set
        if (auto presenter = getPresenter()) {
            operation->setPresenter(presenter);
        }
    }
}

void CompositeOperation::removeSubOperation(const std::string& name) {
    subOperations_.erase(name);
}

bool CompositeOperation::hasSubOperation(const std::string& name) const {
    return subOperations_.find(name) != subOperations_.end();
}

std::vector<std::string> CompositeOperation::getSubOperationNames() const {
    std::vector<std::string> names;
    names.reserve(subOperations_.size());
    for (const auto& [name, _] : subOperations_) {
        names.push_back(name);
    }
    return names;
}

std::shared_ptr<Operation> CompositeOperation::getSubOperation(const std::string& name) const {
    auto it = subOperations_.find(name);
    return (it != subOperations_.end()) ? it->second : nullptr;
}

void CompositeOperation::execute(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "help" || args[0] == "--help") {
        displayHelp();
        return;
    }

    const std::string& subcommand = args[0];
    auto operation = getSubOperation(subcommand);

    if (!operation) {
        auto presenter = getPresenter();
        if (presenter) {
            std::stringstream ss;
            ss << "Unknown subcommand: '" << subcommand << "'";
            presenter->displayError(ss.str());
            presenter->displayInfo("Run with 'help' to see available subcommands");
        }
        return;
    }

    // Create new args without the subcommand
    std::vector<std::string> subArgs(args.begin() + 1, args.end());
    operation->execute(subArgs);
}

void CompositeOperation::setPresenter(std::shared_ptr<Presenter> presenter) {
    Operation::setPresenter(presenter);
    // Propagate to all sub-operations
    for (auto& [_, operation] : subOperations_) {
        if (operation) {
            operation->setPresenter(presenter);
        }
    }
}

void CompositeOperation::displayHelp() const {
    auto presenter = getPresenter();
    if (!presenter) {
        return;
    }

    presenter->displayInfo("Available subcommands:");
    auto names = getSubOperationNames();
    std::sort(names.begin(), names.end());

    for (const auto& name : names) {
        std::stringstream ss;
        ss << "  " << name;
        presenter->displayInfo(ss.str());
    }
}

} // namespace scrap