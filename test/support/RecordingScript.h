#pragma once

#include "support/TempDirectory.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::TestSupport {

/**
 * @brief Write a program that records the arguments it was given.
 *
 * The program writes its arguments, one to a line, to the file "arguments" in
 * the directory it runs in, and then runs @p rest.
 *
 * @param temp Directory to write the program in.
 * @param relative Path of the program relative to @p temp.
 * @param rest Shell commands to run after the arguments are recorded.
 * @return Full path of the written program.
 */
[[nodiscard]] std::filesystem::path writeRecordingScript(const TempDirectory& temp, std::string_view relative, std::string_view rest);

/**
 * @brief The lines of a file, without their newlines.
 *
 * Reports a test failure and returns no lines if the file cannot be read.
 *
 * @param file Path of the file.
 */
[[nodiscard]] std::vector<std::string> linesOf(const std::filesystem::path& file);

}  // namespace scrap::TestSupport
