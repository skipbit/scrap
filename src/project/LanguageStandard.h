#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace scrap::Project {

/**
 * @brief A C++ language standard a project can be built against.
 */
enum class LanguageStandard : std::uint8_t {
    Cxx11,
    Cxx14,
    Cxx17,
    Cxx20,
    Cxx23,
    Cxx26
};

/**
 * @brief Every standard scrap.toml accepts, oldest first.
 */
[[nodiscard]] auto supportedLanguageStandards() -> std::span<const LanguageStandard>;

/**
 * @brief Read the value package.std is written with.
 *
 * Only the number of a standard is read ("23"). Other spellings, "gnu23"
 * among them, are reserved rather than guessed at.
 *
 * @param text The value as written in scrap.toml.
 * @return The standard, or nothing when the value names none this version
 *         accepts.
 */
[[nodiscard]] auto parseLanguageStandard(std::string_view text) -> std::optional<LanguageStandard>;

/**
 * @brief The number scrap.toml writes the standard as, such as "23".
 */
[[nodiscard]] auto standardNumber(LanguageStandard standard) -> std::string_view;

}  // namespace scrap::Project
