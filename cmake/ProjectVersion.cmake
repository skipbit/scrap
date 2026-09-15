# Derive the project version from git tags.
#
# Usage:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersion.cmake)
#   scrap_version_detect(<version-var> <describe-var> <source-dir>)
#   project(<name> VERSION ${<version-var>} ...)
#
# A release tag is v<major>.<minor>.<patch>, optionally followed by a
# pre-release suffix (v1.0.0-rc1) or build metadata (v1.0.0+build7).
# Components carry no leading zeros, and nothing may follow the triple but
# those: v0.2, v1.2.3.4, v01.2.3 and nightly-2026 are not releases, and do not
# become releases by sitting closer to HEAD than one.
#
# The tags git is asked about are chosen by this pattern, not by a glob. A
# glob cannot express "digits only", so anything narrow enough to run inside
# git would still admit shapes the pattern rejects -- and `describe` returns
# the nearest match, so one such tag would hide every release behind it.
#
# When one commit carries several release tags -- a pre-release promoted to
# its release, say -- the version names the preferred one: the highest
# triple, then the release over its pre-releases and build metadata, then the
# greater name. git has a preference of its own (an annotated tag over a
# lightweight one), but an archive cannot see it, so the rule lives here and
# both paths below choose by it.
#
# A tree with no release tag reports 0.0.0, which is what an unreleased
# checkout is. A tree that is not this project's own repository is not asked
# at all -- git searches upwards, so sources vendored inside another project
# would otherwise report that project's version as ours.
#
# Such a tree may still be an archive git made. .gitattributes has git archive
# fill in cmake/ArchiveVersion.txt with the commit and the refs pointing at it,
# which is how a release tarball names its release with no repository to ask.
# An archive knows only its own commit: one made between releases reports
# 0.0.0 and the commit id, where a checkout of the same commit would count the
# distance from the last release.
#
# Both values are read at configure time, which is the only point where
# project(VERSION) can take them. The generated version source is refreshed
# on every build instead (see cmake/GenerateVersionSource.cmake), so what the
# binary prints follows the tags without waiting for a reconfigure.

# Normalise a release tag to the major.minor.patch triple that
# project(VERSION) accepts. A pre-release suffix is dropped here and stays
# visible in the description. Anything else is 0.0.0.
function(scrap_version_from_tag OUT_VERSION TAG)
    set(component "(0|[1-9][0-9]*)")
    if(TAG MATCHES "^v${component}\\.${component}\\.${component}($|[-+])")
        set(${OUT_VERSION} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    else()
        set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    endif()
endfunction()

# Keep the release tags among ref names as git printed them: TEXT split on
# SEPARATOR, and only entries starting with PREFIX, which is removed.
#
# A name may itself contain the CMake list separator. It is escaped before
# splitting so the name stays in one piece -- split naively, v1.0.0-a;b
# becomes v1.0.0-a, which the pattern accepts and which names no tag -- and
# then skipped: it cannot be handed to git as one argument, and a release tag
# has no reason to use the character.
function(scrap_version_release_tags OUT_TAGS TEXT SEPARATOR PREFIX)
    string(REPLACE ";" "\;" entries "${TEXT}")
    string(REPLACE "${SEPARATOR}" ";" entries "${entries}")
    string(LENGTH "${PREFIX}" prefix_length)

    set(tags "")
    foreach(entry IN LISTS entries)
        if(entry MATCHES ";")
            continue()
        endif()
        if(prefix_length GREATER 0)
            string(FIND "${entry}" "${PREFIX}" at)
            if(NOT at EQUAL 0)
                continue()
            endif()
            string(SUBSTRING "${entry}" ${prefix_length} -1 entry)
        endif()

        scrap_version_from_tag(candidate "${entry}")
        if(NOT candidate STREQUAL "0.0.0")
            list(APPEND tags "${entry}")
        endif()
    endforeach()

    set(${OUT_TAGS} "${tags}" PARENT_SCOPE)
endfunction()

# The tag a commit carrying the release tags in ARGN is named by. Empty when
# ARGN is.
function(scrap_version_preferred_tag OUT_TAG)
    set(best "")
    set(best_version "")
    foreach(tag IN LISTS ARGN)
        scrap_version_from_tag(version "${tag}")
        set(take FALSE)
        if(NOT best)
            set(take TRUE)
        elseif(version VERSION_GREATER best_version)
            set(take TRUE)
        elseif(version VERSION_EQUAL best_version AND NOT best STREQUAL "v${best_version}")
            # The release itself beats its pre-releases. Between two suffixed
            # names the choice only has to be the same everywhere, and the
            # greater name is.
            if(tag STREQUAL "v${version}" OR tag STRGREATER best)
                set(take TRUE)
            endif()
        endif()

        if(take)
            set(best "${tag}")
            set(best_version "${version}")
        endif()
    endforeach()

    set(${OUT_TAG} "${best}" PARENT_SCOPE)
endfunction()

# Describe a tree that has no release tag: the short commit id, marked when
# the working tree carries uncommitted changes. This is what `describe
# --always --dirty` reports for such a tree, without asking it to consider
# tags that are not releases.
function(scrap_describe_commit SOURCE_DIR OUT_DESCRIPTION)
    set(${OUT_DESCRIPTION} "" PARENT_SCOPE)

    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE commit_result
    )
    if(NOT commit_result EQUAL 0 OR NOT commit)
        return()
    endif()

    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} status --porcelain --untracked-files=no
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE changes
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE status_result
    )
    if(status_result EQUAL 0 AND changes)
        set(commit "${commit}-dirty")
    endif()

    set(${OUT_DESCRIPTION} "${commit}" PARENT_SCOPE)
endfunction()

# Ask the repository at SOURCE_DIR. OUT_FOUND is false when there is no git,
# or when SOURCE_DIR is not the root of this project's own repository.
function(scrap_version_from_repository SOURCE_DIR OUT_FOUND OUT_VERSION OUT_DESCRIBE)
    set(${OUT_FOUND} FALSE PARENT_SCOPE)

    # find_program rather than find_package(Git): this runs before project(),
    # where the toolchain file and the find-root settings are not in effect
    # yet, and a plain program lookup is all that is needed.
    find_program(SCRAP_GIT_EXECUTABLE NAMES git)
    if(NOT SCRAP_GIT_EXECUTABLE)
        return()
    endif()

    # The repository has to be this project's own. git walks up the directory
    # tree, so without this a copy vendored inside another checkout would
    # inherit that checkout's tags.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} rev-parse --show-toplevel
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE toplevel
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE toplevel_result
    )
    if(NOT toplevel_result EQUAL 0)
        return()
    endif()
    get_filename_component(toplevel "${toplevel}" REALPATH)
    get_filename_component(own_root "${SOURCE_DIR}" REALPATH)
    if(NOT toplevel STREQUAL own_root)
        return()
    endif()

    set(${OUT_FOUND} TRUE PARENT_SCOPE)
    set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "unknown" PARENT_SCOPE)

    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} tag --list
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE all_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE list_result
    )
    if(list_result EQUAL 0)
        scrap_version_release_tags(release_tags "${all_tags}" "\n" "")
    else()
        set(release_tags "")
    endif()

    if(NOT release_tags)
        # No release tag: the commit id is all there is to report. Asking
        # describe with no selector would let an unrelated tag answer, and an
        # empty --match is not documented behaviour to lean on.
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(commit_description)
            set(${OUT_DESCRIBE} "${commit_description}" PARENT_SCOPE)
        endif()
        return()
    endif()

    set(selectors "")
    foreach(tag IN LISTS release_tags)
        list(APPEND selectors "--match=${tag}")
    endforeach()

    # The nearest commit carrying a release tag, by whichever of its tags git
    # happens to name it.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --abbrev=0 ${selectors}
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE nearest
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE nearest_result
    )
    if(NOT nearest_result EQUAL 0 OR NOT nearest)
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(commit_description)
            set(${OUT_DESCRIBE} "${commit_description}" PARENT_SCOPE)
        endif()
        return()
    endif()

    # Every release tag on that commit, and the one the rule prefers.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} tag --points-at "${nearest}^{commit}"
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE here
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    scrap_version_release_tags(here_tags "${here}" "\n" "")
    scrap_version_preferred_tag(chosen ${here_tags})
    if(NOT chosen)
        set(chosen "${nearest}")
    endif()
    scrap_version_from_tag(version "${chosen}")
    set(${OUT_VERSION} "${version}" PARENT_SCOPE)

    # Distance, commit id and dirtiness from that commit. Matching only the tag
    # git named it by guarantees the description starts with that name, which
    # is then replaced by the preferred one: every tag on the commit is the
    # same distance away.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --always --dirty "--match=${nearest}"
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE describe_result
    )
    string(FIND "${describe}" "${nearest}" at)
    if(describe_result EQUAL 0 AND at EQUAL 0)
        string(LENGTH "${nearest}" nearest_length)
        string(SUBSTRING "${describe}" ${nearest_length} -1 rest)
        set(${OUT_DESCRIBE} "${chosen}${rest}" PARENT_SCOPE)
    else()
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(commit_description)
            set(${OUT_DESCRIBE} "${commit_description}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# Read what git archive filled in at SOURCE_DIR. OUT_FOUND is false when the
# file is missing or its placeholders were never substituted -- a checkout,
# or a copy made some other way.
function(scrap_version_from_archive SOURCE_DIR OUT_FOUND OUT_VERSION OUT_DESCRIBE)
    set(${OUT_FOUND} FALSE PARENT_SCOPE)

    set(archive_file "${SOURCE_DIR}/cmake/ArchiveVersion.txt")
    if(NOT EXISTS "${archive_file}")
        return()
    endif()
    file(READ "${archive_file}" content)
    if(NOT content MATCHES "(^|\n)commit=([0-9a-f]+)")
        return()
    endif()
    set(commit "${CMAKE_MATCH_2}")

    set(refs "")
    if(content MATCHES "(^|\n)refs=([^\n]*)")
        set(refs "${CMAKE_MATCH_2}")
    endif()

    # %D lists the refs on the commit as "HEAD -> main, tag: v0.1.0, ...". Ref
    # names cannot contain a space, so the separator cannot occur inside one.
    scrap_version_release_tags(tags "${refs}" ", " "tag: ")
    scrap_version_preferred_tag(chosen ${tags})

    set(${OUT_FOUND} TRUE PARENT_SCOPE)
    if(chosen)
        scrap_version_from_tag(version "${chosen}")
        set(${OUT_VERSION} "${version}" PARENT_SCOPE)
        set(${OUT_DESCRIBE} "${chosen}" PARENT_SCOPE)
    else()
        set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
        set(${OUT_DESCRIBE} "${commit}" PARENT_SCOPE)
    endif()
endfunction()

# Resolve the numeric version and the full git description of SOURCE_DIR: from
# its repository when it is this project's own, otherwise from what git
# archive left behind, otherwise 0.0.0 and an unknown revision.
function(scrap_version_detect OUT_VERSION OUT_DESCRIBE SOURCE_DIR)
    scrap_version_from_repository("${SOURCE_DIR}" found detected_version detected_describe)
    if(NOT found)
        scrap_version_from_archive("${SOURCE_DIR}" found detected_version detected_describe)
    endif()
    if(NOT found)
        set(detected_version "0.0.0")
        set(detected_describe "unknown")
    endif()

    set(${OUT_VERSION} "${detected_version}" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "${detected_describe}" PARENT_SCOPE)
endfunction()

# Compose the banner shown to users. A build standing exactly on a release tag
# says only the release; anything else carries the description, so a binary
# built between releases can be traced back to the commit it came from.
function(scrap_version_banner OUT_BANNER NAME VERSION DESCRIBE)
    if(DESCRIBE STREQUAL "v${VERSION}")
        set(${OUT_BANNER} "${NAME} ${VERSION}" PARENT_SCOPE)
    else()
        set(${OUT_BANNER} "${NAME} ${VERSION} (${DESCRIBE})" PARENT_SCOPE)
    endif()
endfunction()
