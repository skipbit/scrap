#pragma once

#include <expected>  // IWYU pragma: keep
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scrap::Toolchain {

/// The name the compiler is asked to resolve to the archiver it uses.
inline constexpr std::string_view DefaultArchiverName = "ar";

/**
 * @brief Why no archiver was settled on.
 */
struct NoArchiver {
    std::optional<std::string> named;  ///< What the compiler answered; nothing when it could not be asked.
};

/**
 * @brief Find the archiver that goes with @p compiler.
 *
 * The compiler is asked with -print-prog-name=ar, which names the archiver it
 * would itself run, by the rule it uses to find its linker: a compiler that
 * keeps one beside itself answers with that path, and one that does not
 * answers with the bare name. A path is read as it stands; a bare name is
 * looked for on @p systemSearchPaths. Whichever way it was found, the answer
 * is absolute.
 *
 * A compiler that cannot be asked, or whose answer cannot be run, is
 * reported rather than replaced by another archiver: a library written by
 * one the compiler did not name would be a build nobody asked for.
 *
 * @param compiler The compiler in use, as an absolute path.
 * @param systemSearchPaths Directories PATH lists, in order.
 * @return The archiver to run, or what was named and could not be run.
 */
[[nodiscard]] std::expected<std::filesystem::path, NoArchiver> findArchiver(const std::filesystem::path& compiler,
                                                                            const std::vector<std::filesystem::path>& systemSearchPaths);

}  // namespace scrap::Toolchain
