#pragma once

#include "command/RuntimeEnvironment.h"

#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief What the process was started with, as read from it: the working
 *        directory and the environment variables scrap reads, each empty
 *        when unset.
 */
struct ProcessInputs {
    std::filesystem::path workingDirectory;
    std::string scrapHome;        ///< SCRAP_HOME
    std::string home;             ///< HOME
    std::string path;             ///< PATH, colon-separated.
    std::string compiler;         ///< CXX
    std::string toolchain;        ///< SCRAP_TOOLCHAIN
    std::string toolchainIndex;   ///< SCRAP_TOOLCHAIN_INDEX
    std::string certificateFile;  ///< SSL_CERT_FILE
    std::string sdkRoot;          ///< SDKROOT
};

/**
 * @brief Build a RuntimeEnvironment from what the process was started with.
 *
 * Pure function: every field is derived solely from @p inputs, without
 * reading environment variables or touching the filesystem. Callers (e.g.
 * main()) are responsible for reading them and for resolving the current
 * working directory.
 *
 * scrapHome is SCRAP_HOME when it is not empty, otherwise ".scrap" below
 * HOME when HOME is not empty, otherwise nothing. A relative path is taken
 * against the working directory, so scrapHome is absolute whenever the
 * working directory is: the same relative SCRAP_HOME names a different
 * directory in each directory a command is run from.
 *
 * Empty PATH segments (from leading, trailing, or doubled colons) are
 * skipped. CXX is carried as preferredCompiler, and SCRAP_TOOLCHAIN as
 * requestedToolchain.
 */
[[nodiscard]] RuntimeEnvironment makeRuntimeEnvironment(const ProcessInputs& inputs);

}  // namespace scrap::Command
