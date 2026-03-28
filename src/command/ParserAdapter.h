#pragma once

#include "command/CommandSpec.h"
#include "command/ParseResult.h"

#include <span>

namespace scrap::Command {

class ParserAdapter {
public:
    virtual ~ParserAdapter();
    virtual auto configure(std::span<const CommandSpec> specs) -> void = 0;
    virtual auto parse(std::span<const char* const> argv) const -> ParseResult = 0;
};

}  // namespace scrap::Command
