#include "toolchain/ScrapHomeLayout.h"

#include <filesystem>
#include <string_view>

namespace scrap::Toolchain {

std::filesystem::path toolchainsDirectory(const std::filesystem::path& scrapHome)
{
    return scrapHome / "toolchains";
}

std::filesystem::path familyDirectory(const std::filesystem::path& scrapHome, const std::string_view family)
{
    return toolchainsDirectory(scrapHome) / family;
}

std::filesystem::path toolchainDirectory(const std::filesystem::path& scrapHome, const std::string_view family, const std::string_view version, const std::string_view environment)
{
    return familyDirectory(scrapHome, family) / version / environment;
}

std::filesystem::path indexCopyFile(const std::filesystem::path& scrapHome)
{
    return scrapHome / "index" / "toolchain.toml";
}

std::filesystem::path temporaryDirectory(const std::filesystem::path& scrapHome)
{
    return scrapHome / "tmp";
}

std::filesystem::path globalConfigFile(const std::filesystem::path& scrapHome)
{
    return scrapHome / "config.toml";
}

}  // namespace scrap::Toolchain
