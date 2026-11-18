#include "command/ParsedOptions.h"

#include <string>
#include <string_view>
#include <variant>

namespace scrap::Command {

bool isGiven(const ParsedOptions& options, const std::string_view name)
{
    const auto option = options.named.find(std::string{ name });
    if (option == options.named.end()) {
        return false;
    }
    const bool* const given = std::get_if<bool>(&option->second);
    return (given != nullptr) && *given;
}

}  // namespace scrap::Command
