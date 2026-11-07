#include "toolchain/RuntimeLibraryName.h"

#include <string>

namespace scrap::Toolchain {

bool isLibcxxName(const std::string& name)
{
    return name.starts_with("libc++.so") || (name.starts_with("libc++.") && name.ends_with(".dylib"));
}

}  // namespace scrap::Toolchain
