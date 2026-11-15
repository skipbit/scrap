#include "install/UtcTime.h"

#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

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

bool isUtcText(const std::string_view text)
{
    constexpr std::string_view Shape = "0000-00-00T00:00:00Z";
    if (text.size() != Shape.size()) {
        return false;
    }
    for (std::size_t i = 0; i < Shape.size(); ++i) {
        const bool fits = (Shape[i] == '0') ? (std::isdigit(static_cast<unsigned char>(text[i])) != 0) : (text[i] == Shape[i]);
        if (! fits) {
            return false;
        }
    }
    return true;
}

}  // namespace scrap::Install
