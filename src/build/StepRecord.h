#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Build {

/**
 * @brief A file a step read, as it stood when the step ran.
 */
struct InputStamp {
    std::filesystem::path path;  ///< The file, as the step named it.
    std::int64_t modified = 0;   ///< When it was last written, in nanoseconds since the epoch.
    std::uintmax_t size = 0;     ///< Its size in bytes.

    friend bool operator==(const InputStamp&, const InputStamp&) = default;
};

/**
 * @brief What a step that succeeded ran, and what it read.
 *
 * A later build compares it with the step it is about to run, to tell
 * whether running the step again would write anything different.
 */
struct StepRecord {
    std::vector<std::string> arguments;  ///< The command line, starting with the program.
    std::vector<InputStamp> inputs;      ///< Every file the step read.
    /// Paths that held no file when the step ran, where a file that appears
    /// would be read in place of one of the inputs.
    std::vector<std::filesystem::path> absent;

    friend bool operator==(const StepRecord&, const StepRecord&) = default;
};

/**
 * @brief Render @p record as text, a line per item.
 *
 * The first line names the format and its version, and the last counts the
 * arguments, the inputs and the absent paths, so a record cut short is not
 * taken for one that read fewer files. A backslash or a line break within an argument or a path
 * is written as an escape, so every item stays on a line of its own.
 *
 * @return The text, ending in a newline.
 */
[[nodiscard]] std::string renderStepRecord(const StepRecord& record);

/**
 * @brief Read a record that renderStepRecord() wrote.
 *
 * @return The record, or nothing when @p text is not a whole record of this
 *         version: written by another version, cut short, or changed in a
 *         way the format does not allow.
 */
[[nodiscard]] std::optional<StepRecord> parseStepRecord(std::string_view text);

}  // namespace scrap::Build
