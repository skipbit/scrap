#pragma once

#include "command/CommandSpec.h"
#include "command/ParseResult.h"

#include <span>

namespace scrap::Command {

class ParserAdapter {
public:
    virtual ~ParserAdapter();
    ParserAdapter(const ParserAdapter&) = default;
    ParserAdapter& operator=(const ParserAdapter&) = default;
    ParserAdapter(ParserAdapter&&) = default;
    ParserAdapter& operator=(ParserAdapter&&) = default;

    virtual auto configure(std::span<const CommandSpec> specs) -> void = 0;
    [[nodiscard]] virtual auto parse(std::span<const char* const> argv) const -> ParseResult = 0;

protected:
    ParserAdapter() = default;
};

}  // namespace scrap::Command
