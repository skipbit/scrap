#include "project/LanguageStandard.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace scrap::Project {

namespace {

constexpr std::array<LanguageStandard, 6> SupportedStandards{ LanguageStandard::Cxx11, LanguageStandard::Cxx14, LanguageStandard::Cxx17, LanguageStandard::Cxx20, LanguageStandard::Cxx23, LanguageStandard::Cxx26 };

}  // anonymous namespace

std::span<const LanguageStandard> supportedLanguageStandards()
{
    return SupportedStandards;
}

std::optional<LanguageStandard> parseLanguageStandard(std::string_view text)
{
    for (const LanguageStandard standard : SupportedStandards) {
        if (standardNumber(standard) == text) {
            return standard;
        }
    }
    return std::nullopt;
}

std::string_view standardNumber(const LanguageStandard standard)
{
    switch (standard) {
    case LanguageStandard::Cxx11:
        return "11";
    case LanguageStandard::Cxx14:
        return "14";
    case LanguageStandard::Cxx17:
        return "17";
    case LanguageStandard::Cxx20:
        return "20";
    case LanguageStandard::Cxx23:
        return "23";
    case LanguageStandard::Cxx26:
        return "26";
    }
    std::unreachable();
}

}  // namespace scrap::Project
