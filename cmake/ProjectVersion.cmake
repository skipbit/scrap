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
# Listing the tags, filtering them here, and naming the survivors gives both
# describe calls the same set by construction.
#
# A tree with no release tag reports 0.0.0, which is what an unreleased
# checkout is. So does a tree that is not this project's own repository --
# git searches upwards, so sources vendored inside another project would
# otherwise report that project's version as ours.
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

# Resolve the numeric version and the full git description of SOURCE_DIR.
function(scrap_version_detect OUT_VERSION OUT_DESCRIBE SOURCE_DIR)
    set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "unknown" PARENT_SCOPE)

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

    # Every tag, filtered by the pattern.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} tag --list
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE all_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE list_result
    )
    if(NOT list_result EQUAL 0)
        return()
    endif()

    # git separates names by newline, but a name may itself contain the CMake
    # list separator. Escaping it first keeps such a name in one piece: split
    # naively, v1.0.0-a;b becomes v1.0.0-a, which the pattern accepts and
    # which names no tag at all -- the release would vanish behind a selector
    # matching nothing.
    string(REPLACE ";" "\\;" all_tags "${all_tags}")
    string(REPLACE "\n" ";" all_tags "${all_tags}")
    set(selectors "")
    foreach(tag IN LISTS all_tags)
        # A name carrying the CMake list separator cannot be handed to git as
        # one argument, so it cannot be selected. git permits the character;
        # a release tag has no reason to use it, and passing such a name
        # through would split into a selector naming no tag at all.
        if(tag MATCHES ";")
            continue()
        endif()

        scrap_version_from_tag(candidate "${tag}")
        if(NOT candidate STREQUAL "0.0.0")
            list(APPEND selectors "--match=${tag}")
        endif()
    endforeach()

    if(NOT selectors)
        # No release tag: the commit id is all there is to report. Asking
        # describe with no selector would let an unrelated tag answer, and an
        # empty --match is not documented behaviour to lean on.
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(commit_description)
            set(${OUT_DESCRIBE} "${commit_description}" PARENT_SCOPE)
        endif()
        return()
    endif()

    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --abbrev=0 ${selectors}
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE tag_result
    )
    if(tag_result EQUAL 0)
        scrap_version_from_tag(version "${tag}")
        set(${OUT_VERSION} "${version}" PARENT_SCOPE)
    endif()

    # The same set of tags, so both halves describe the same one.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --always --dirty ${selectors}
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE describe_result
    )
    if(describe_result EQUAL 0 AND describe)
        set(${OUT_DESCRIBE} "${describe}" PARENT_SCOPE)
    else()
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(commit_description)
            set(${OUT_DESCRIBE} "${commit_description}" PARENT_SCOPE)
        endif()
    endif()
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
