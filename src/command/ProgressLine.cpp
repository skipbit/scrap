#include "command/ProgressLine.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace scrap::Command {

namespace {

/// Width the word for what is being done is written in, so the lines line up.
constexpr std::size_t VerbWidth = 12;

}  // anonymous namespace

std::string alignedVerb(const std::string_view verb)
{
    std::string text(VerbWidth - std::min(VerbWidth, verb.size()), ' ');
    text += verb;
    return text;
}

}  // namespace scrap::Command
