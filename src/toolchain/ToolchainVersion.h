#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

/**
 * @brief A version written as numbers joined by dots, such as 15.2.0.
 *
 * Each part is compared as a number of any length, so 015 and 15 are the
 * same part and 150 comes after 15.
 */
class ToolchainVersion {
public:
    /**
     * @brief Read @p text as a version.
     *
     * @return The version, or nothing when @p text is empty, holds anything
     *         but digits and dots, or has an empty part (15., .15, 15..2).
     */
    [[nodiscard]] static std::optional<ToolchainVersion> parse(std::string_view text);

    /** The version as it was written. */
    [[nodiscard]] const std::string& text() const;

    /**
     * @brief Whether @p prefix names this version or a series it belongs to.
     *
     * The match is by whole parts: 15 starts 15.2.0 but not 150.1.0.
     */
    [[nodiscard]] bool startsWith(const ToolchainVersion& prefix) const;

    /**
     * Part by part as numbers; a version that runs out of parts first, being
     * a prefix of the other, comes first.
     */
    [[nodiscard]] std::strong_ordering operator<=>(const ToolchainVersion& other) const;
    [[nodiscard]] bool operator==(const ToolchainVersion& other) const;

private:
    ToolchainVersion(std::string text, std::vector<std::string> parts);

    std::string _text;
    std::vector<std::string> _parts;  ///< Each part without its leading zeros, "0" for zero.
};

}  // namespace scrap::Toolchain
