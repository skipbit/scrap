#pragma once

#include <span>
#include <string>
#include <map>
#include <memory>

namespace scrap {

class Operation;

template<typename T>
concept OperationType = std::derived_from<T, Operation>;

class Command {
    template<OperationType T, class... Args>
    friend Command makeCommand(Args&&...);
public:
    Command(const Command&);
    virtual ~Command();

    void add(const std::string& key, const Command& cmd);
    void remove(const std::string& key);

    void execute(const int argc, const char* const argv[]);
    void execute(const std::span<const std::string>& arguments);

    class Option {
    public:
        Option();
        Option(const Option&);
        virtual ~Option();
    };

    Command& operator=(const Command&);

private:
    std::shared_ptr<Operation> operation_;
    std::map<std::string, Command> commands_;

    Command(std::shared_ptr<Operation>);
};

template <OperationType T, class... Args>
Command makeCommand(Args&&... args) {
    return Command(std::make_shared<T>(std::forward<Args>(args)...));
}

}
