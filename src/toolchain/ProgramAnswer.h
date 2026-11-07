#pragma once

#include "process/Subprocess.h"

#include <optional>
#include <string>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief The one line @p arguments answer with, without its line end, or
 *        nothing when the program cannot be run, fails or prints nothing.
 *
 * @param arguments The program and its arguments.
 * @param options How the program is run; its standard output is read.
 */
[[nodiscard]] std::optional<std::string> askProgram(const std::vector<std::string>& arguments, const Process::RunOptions& options);

}  // namespace scrap::Toolchain
