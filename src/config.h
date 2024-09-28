#pragma once

#include <string>
#include <optional>

namespace scrap {

class config {
public:
    static std::optional<config> default_config();
    config(const std::string&);

private:
    std::string _directory;
};

}
