#pragma once

#include <optional>
#include <string>

namespace scrap {

class environment {
public:
    static std::optional<std::string> value(const std::string&);
};

}
