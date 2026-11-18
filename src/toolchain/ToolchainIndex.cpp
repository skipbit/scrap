#include "toolchain/ToolchainIndex.h"

#include <string>

namespace scrap::Toolchain {

std::string IndexedToolchain::text() const
{
    return family + "@" + version.text();
}

}  // namespace scrap::Toolchain
