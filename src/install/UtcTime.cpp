#include "install/UtcTime.h"

#include <array>
#include <chrono>
#include <ctime>
#include <optional>
#include <string>

namespace scrap::Install {

std::optional<std::string> utcText(const std::chrono::system_clock::time_point time)
{
    const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
    std::tm parts{};
    std::array<char, 32> text{};
    if ((::gmtime_r(&seconds, &parts) == nullptr) || (std::strftime(text.data(), text.size(), "%Y-%m-%dT%H:%M:%SZ", &parts) == 0)) {
        return std::nullopt;
    }
    return std::string{ text.data() };
}

}  // namespace scrap::Install
