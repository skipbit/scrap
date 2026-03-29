#pragma once

#include <string>

namespace scrap::Command {

class VersionRenderer {
public:
    virtual ~VersionRenderer();
    VersionRenderer(const VersionRenderer&) = default;
    VersionRenderer& operator=(const VersionRenderer&) = default;
    VersionRenderer(VersionRenderer&&) = default;
    VersionRenderer& operator=(VersionRenderer&&) = default;

    virtual auto render() const -> std::string = 0;

protected:
    VersionRenderer() = default;
};

}  // namespace scrap::Command
