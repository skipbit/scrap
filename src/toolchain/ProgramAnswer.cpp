#include "toolchain/ProgramAnswer.h"

#include "process/Subprocess.h"

#include <expected>  // IWYU pragma: keep
#include <optional>
#include <string>
#include <vector>

namespace scrap::Toolchain {

std::optional<std::string> askProgram(const std::vector<std::string>& arguments, const Process::RunOptions& options)
{
    const auto completion = Process::runProgram(arguments, options);
    if ((! completion.has_value()) || (completion->exitCode != 0)) {
        return std::nullopt;
    }
    std::string answer = completion->output;
    while ((! answer.empty()) && ((answer.back() == '\n') || (answer.back() == '\r'))) {
        answer.pop_back();
    }
    if (answer.empty()) {
        return std::nullopt;
    }
    return answer;
}

}  // namespace scrap::Toolchain
