#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace scrap::Tar {

/**
 * @brief The components of the path of an archive member, without "." and empty
 *        ones.
 *
 * The result refers to the bytes of @p path. An archive's root directory itself
 * ("./") has no components.
 *
 * @return The components, or nothing when the path is empty, absolute or holds
 *         "..", and so does not name a place inside the archive.
 */
[[nodiscard]] std::optional<std::vector<std::string_view>> memberComponents(std::string_view path);

/**
 * @brief Whether a symbolic link at @p link, pointing at @p target, stays inside
 *        the directory the archive is extracted into.
 *
 * The target must be relative, with any ".." at its start and no more of them
 * than the directories above the link. A ".." after a name is refused however
 * it looks: the name may turn out to be another link, so the path as written
 * says nothing about where it leads.
 *
 * Together with no member being written through a link, this keeps every link
 * inside: the ".." walk up real directories, and each name below leads to a real
 * entry or to a link kept inside in the same way.
 */
[[nodiscard]] bool symbolicLinkStaysInside(std::span<const std::string_view> link, std::string_view target);

}  // namespace scrap::Tar
