#pragma once

#include <cstdint>

namespace scrap::Command {

enum class CommandSource : std::uint8_t {
    Builtin,
    External,
    Project
};

}  // namespace scrap::Command
