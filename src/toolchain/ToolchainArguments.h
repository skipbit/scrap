#pragma once

#include <string>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief What a compiler is given to build with a toolchain.
 */
struct ToolchainArguments {
    std::vector<std::string> compile;  ///< Given to every compile.
    std::vector<std::string> link;     ///< Given to every link.
};

}  // namespace scrap::Toolchain
