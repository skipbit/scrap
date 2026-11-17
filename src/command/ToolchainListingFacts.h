#pragma once

#include "command/RuntimeEnvironment.h"
#include "command/ToolchainListing.h"

#include <expected>
#include <filesystem>
#include <string>

namespace scrap::Command {

/**
 * @brief Read what a listing of the toolchains to choose from is made of.
 *
 * What names the default or the toolchain used here and cannot be read,
 * and a CXX that cannot be used, are warned about on standard error, and
 * the rest is read. On a host no toolchain is built for, none is
 * installed.
 *
 * @return The facts, or the error lines when the installed toolchains
 *         below @p scrapHome cannot be listed.
 */
[[nodiscard]] std::expected<ListingFacts, std::string> readListingFacts(const RuntimeEnvironment& env, const std::filesystem::path& scrapHome);

}  // namespace scrap::Command
