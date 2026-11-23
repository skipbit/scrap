#pragma once

#include <string>
#include <string_view>

namespace scrap::Command {

/**
 * @brief @p verb right-aligned in the width the progress lines of every
 *        command line up in, as in "   Compiling" and "    Finished".
 *
 * A verb longer than the width is written as it is, so the lines lose their
 * alignment rather than the verb being cut.
 */
[[nodiscard]] std::string alignedVerb(std::string_view verb);

}  // namespace scrap::Command
