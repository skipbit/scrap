#pragma once

#include "command/ParsedOptions.h"

#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>

namespace scrap::Command {

struct CommandInvocation {
    std::vector<std::string> commandPath;  ///< Names of the command and the commands above it, outermost first.
    ParsedOptions options;
};

enum class ParseDirectiveKind : std::uint8_t {
    HelpRequested,
    VersionRequested
};

struct ParseDirective {
    ParseDirectiveKind kind = ParseDirectiveKind::HelpRequested;
    std::vector<std::string> target;  ///< Words naming the command to show; none for the global help.
};

/**
 * A command line the commands cannot take.
 */
struct ParseFailure {
    std::string message;
};

/**
 * A fault in the parser or in the commands it was given, not in the command line.
 */
struct ParserFault {
    std::string message;
};

using ParseInterruption = std::variant<ParseDirective, ParseFailure, ParserFault>;

using ParseResult = std::expected<CommandInvocation, ParseInterruption>;

}  // namespace scrap::Command
