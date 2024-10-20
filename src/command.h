#pragma once

#include <span>
#include <string>
#include <map>
#include <memory>

namespace scrap {

class operation;

template<typename T>
concept operation_type = std::derived_from<T, operation>;

class command {
    template<operation_type T, class... Args>
    friend command make_command(Args&&...);
public:
    command(const command&);
    virtual ~command();

    void add(const std::string& key, const command& cmd);
    void remove(const std::string& key);

    void execute(const int argc, const char* const argv[]);
    void execute(const std::span<const std::string>& arguments);

    class option {
    public:
        option();
        option(const option&);
        virtual ~option();
    };

    command& operator=(const command&);

private:
    std::shared_ptr<operation> _operation;
    std::map<std::string, command> _commands;

    command(std::shared_ptr<operation>);
};

template <operation_type T, class... Args>
command make_command(Args&&... args) {
    return command(std::make_shared<T>(std::forward<Args>(args)...));
}

}
