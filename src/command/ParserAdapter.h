#pragma once

#include "command/CommandSpec.h"
#include "command/ParseResult.h"

#include <span>
#include <string_view>

namespace scrap::Command {

class ParserAdapter {
public:
    virtual ~ParserAdapter();
    ParserAdapter(const ParserAdapter&) = default;
    ParserAdapter& operator=(const ParserAdapter&) = default;
    ParserAdapter(ParserAdapter&&) = default;
    ParserAdapter& operator=(ParserAdapter&&) = default;

    virtual void configure(std::span<const CommandSpec> specs) = 0;
    [[nodiscard]] virtual ParseResult parse(std::span<const char* const> argv) const = 0;

    /**
     * @brief Tell whether a command can take @p name.
     *
     * A caller passes to configure() only commands whose names it accepts.
     */
    [[nodiscard]] virtual bool acceptsName(std::string_view name) const = 0;

protected:
    ParserAdapter() = default;
};

}  // namespace scrap::Command
