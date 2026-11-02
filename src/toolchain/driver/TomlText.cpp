#include "toolchain/driver/TomlText.h"

#include <toml++/toml.hpp>

#include <sstream>
#include <string>

namespace scrap::Toolchain {

std::string tomlText(const toml::table& table)
{
    std::ostringstream written;
    written << table;
    std::string text = written.str();
    if (! text.empty()) {
        text += '\n';
    }
    return text;
}

}  // namespace scrap::Toolchain
