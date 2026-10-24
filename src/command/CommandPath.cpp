#include "command/CommandPath.h"

#include <cstddef>
#include <span>
#include <string>

namespace scrap::Command {

std::string typedPath(std::span<const std::string> names)
{
    std::string text;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index > 0) {
            text += ' ';
        }
        text += names[index];
    }
    return text;
}

}  // namespace scrap::Command
