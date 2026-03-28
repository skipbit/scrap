#pragma once

#include <filesystem>
#include <vector>

namespace scrap::Command {

struct RuntimeEnvironment {
    std::filesystem::path projectRoot;
    std::vector<std::filesystem::path> searchPaths;
};

}  // namespace scrap::Command
