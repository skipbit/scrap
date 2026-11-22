#include "toolchain/CommandArgument.h"

#include <filesystem>

namespace scrap::Toolchain {

std::filesystem::path pathAsArgument(const std::filesystem::path& path)
{
    if (path.is_relative() && (path.native().starts_with('-') || path.native().starts_with('@'))) {
        return std::filesystem::path{ "." } / path;
    }
    return path;
}

}  // namespace scrap::Toolchain
