#pragma once

#include "command/RemoteListing.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Command {

/**
 * @brief @p rows as a table, with a note of why the host cannot install a
 *        version when @p withNotes, followed by when the index was fetched.
 *
 * @param fetchedAt When the index was fetched, in UTC; nothing when unknown.
 */
[[nodiscard]] std::string renderRemoteListingTable(const std::vector<RemoteToolchain>& rows, const std::optional<std::string>& fetchedAt, bool withNotes);

/**
 * @brief The lines that say no toolchain of the index can be installed on
 *        @p environment, with how to list them all.
 */
[[nodiscard]] std::string renderNothingInstallableHere(std::string_view environment);

/**
 * @brief The error and hint lines for a listing of an index that was never
 *        fetched.
 */
[[nodiscard]] std::string renderIndexNotFetched();

/**
 * @brief The error and hint lines for --all given without --remote.
 */
[[nodiscard]] std::string renderAllWithoutRemote();

}  // namespace scrap::Command
