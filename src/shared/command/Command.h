#pragma once

#include <CLI/CLI.hpp>
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

    // New parser-based execution
    void run(int argc, const char* const argv[]);

    // Access to subcommands for parser configuration
    const std::map<std::string, Command>& getSubcommands() const;

    // Setup operation (needed by parser for recursive configuration)
    void setupOperation();


    Command& operator=(const Command&);

private:
    std::shared_ptr<Operation> operation_;
    std::map<std::string, Command> commands_;

    Command(std::shared_ptr<Operation>);

    // Internal dispatch method
    void dispatch(CLI::App* app, const std::vector<std::string>& remainingArgs);
    
    // Configure CLI11 app recursively
    void configureApp(CLI::App* app);
};

template <OperationType T, class... Args>
Command makeCommand(Args&&... args) {
    return Command(std::make_shared<T>(std::forward<Args>(args)...));
}

}
