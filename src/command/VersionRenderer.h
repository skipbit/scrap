#pragma once

#include <string>

namespace scrap::Command {

class VersionRenderer {
public:
    virtual ~VersionRenderer();
    virtual auto render() const -> std::string = 0;
};

}  // namespace scrap::Command
