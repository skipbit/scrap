#pragma once

#include "command/ParsedOptions.h"

#include <expected>
#include <optional>
#include <string>
#include <variant>

namespace scrap::Command {

struct CommandInvocation {
    std::string commandPath;
    ParsedOptions options;
};

enum class ParseDirectiveKind {
    HelpRequested,
    VersionRequested
};

struct ParseDirective {
    ParseDirectiveKind kind;
    std::optional<std::string> target;
};

struct ParseFailure {
    std::string message;
};

using ParseInterruption = std::variant<ParseDirective, ParseFailure>;

using ParseResult = std::expected<CommandInvocation, ParseInterruption>;

}  // namespace scrap::Command
