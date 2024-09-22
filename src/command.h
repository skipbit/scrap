#pragma once

#include <map>
#include <span>
#include <string>

namespace scrap {

class operation;

class command {
public:
    command();
    command(const command&);
    virtual ~command();

    void add(const std::string& key, const scrap::operation& operation);
    void remove(const std::string& key);

    void execute(const std::span<const std::string>& arguments);

private:
    std::map<std::string, scrap::operation> _operations;
};

}
