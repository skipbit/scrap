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
# In a checkout, the tags git is asked about are chosen by this pattern, not by
# a glob. A glob cannot express "digits only", so anything narrow enough to run
# inside git would still admit shapes the pattern rejects -- and `describe`
# returns the nearest match, so one such tag would hide every release behind
# it. Listing the tags, filtering them here, and naming the survivors gives
# both describe calls the same set by construction.
#
# A tree with no release tag reports 0.0.0, which is what an unreleased
# checkout is. A tree that is not this project's own repository is not asked
# -- git searches upwards, so sources vendored inside another project would
# otherwise report that project's version as ours.
#
# A source archive has no repository to ask. .gitattributes has git archive
# fill in cmake/ArchiveVersion.txt with the commit and its description, which
# is how the tarball GitHub attaches to a release names that release. Only
# placeholders whose output does not move when branches do are used: a list
# of the refs on the commit would change the archive's bytes every time a
# branch moved, breaking anyone who pins its checksum. The description still
# changes if another matching tag is later put on the same commit.
#
# That description is chosen by a glob, because nothing else runs inside git
# archive, and a glob cannot express "digits only". A malformed tag the glob
# admits -- nearer than the last release, or on the release commit and
# preferred by git -- makes an archive report 0.0.0 where a checkout finds the
# release: no version, rather than a wrong one. Excluding such shapes in the
# glob would exclude pre-releases like v1.0.0-rc.1 along with them. Names
# carrying the CMake list separator are excluded in the glob and rejected
# here, as in a checkout; the pattern alone would accept v1.0.0-a;b.
#
# The repository answers when it knows a release, and a filled-in archive file
# answers otherwise. That covers a repository someone started on top of an
# unpacked archive: until it tags a release of its own, the file still says
# where the tree came from. Such a repository's own archives are not covered --
# the file it committed is already filled in, so they carry the original
# description.
#
# Commit ids are abbreviated to 12 characters in both paths rather than to
# git's automatic length, which grows with the repository and would change an
# old archive's bytes. git lengthens 12 when a shorter prefix is ambiguous; an
# archive's bare commit id, cut from the full id, cannot follow it there.
#
# Both values are read at configure time, which is the only point where
# project(VERSION) can take them. The generated version source is refreshed
# on every build instead (see cmake/GenerateVersionSource.cmake), so what the
# binary prints follows the tags without waiting for a reconfigure.
#
# Values are compared with STREQUAL rather than tested for truth: CMake reads
# any string ending in -NOTFOUND as false, and v1.0.0-NOTFOUND is a release
# tag under the pattern above.

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

# Describe a tree that has no release tag: the commit id, marked when the
# working tree carries uncommitted changes. This is what `describe --always
# --dirty` reports for such a tree, without asking it to consider tags that
# are not releases.
function(scrap_describe_commit SOURCE_DIR OUT_DESCRIPTION)
    set(${OUT_DESCRIPTION} "" PARENT_SCOPE)

    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} rev-parse --short=12 HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE commit_result
    )
    if(NOT commit_result EQUAL 0 OR commit STREQUAL "")
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
    if(status_result EQUAL 0 AND NOT changes STREQUAL "")
        set(commit "${commit}-dirty")
    endif()

    set(${OUT_DESCRIPTION} "${commit}" PARENT_SCOPE)
endfunction()

# Read what git archive filled in at SOURCE_DIR. OUT_FOUND is false when the
# file is missing or its placeholders were never substituted -- a checkout, or
# a copy made some other way.
function(scrap_version_from_archive SOURCE_DIR OUT_FOUND OUT_VERSION OUT_DESCRIBE)
    set(${OUT_FOUND} FALSE PARENT_SCOPE)

    set(archive_file "${SOURCE_DIR}/cmake/ArchiveVersion.txt")
    if(NOT EXISTS "${archive_file}")
        return()
    endif()
    file(READ "${archive_file}" content)
    if(NOT content MATCHES "(^|\n)commit=([0-9a-f]+)(\r|\n|$)")
        return()
    endif()
    set(commit "${CMAKE_MATCH_2}")
    string(LENGTH "${commit}" commit_length)
    if(commit_length LESS 12)
        return()
    endif()
    string(SUBSTRING "${commit}" 0 12 commit)

    set(description "")
    if(content MATCHES "(^|\n)describe=([^\n]*)")
        string(STRIP "${CMAKE_MATCH_2}" description)
    endif()

    set(${OUT_FOUND} TRUE PARENT_SCOPE)
    set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "${commit}" PARENT_SCOPE)

    # A name carrying the list separator is rejected as in a checkout. The
    # glob excludes it already; this holds for a file filled in by an older
    # template or by hand.
    if(description MATCHES ";")
        return()
    endif()

    # The description is the release pattern's to judge, with no parsing
    # first: the pattern already accepts what follows a tag in a description
    # (v0.1.0-3-g...), and rejects everything else the file can hold -- an
    # empty value when no tag matched, the placeholder itself when the git that
    # made the archive did not expand it, or a malformed tag the glob let
    # through.
    scrap_version_from_tag(version "${description}")
    if(version STREQUAL "0.0.0")
        return()
    endif()

    set(${OUT_VERSION} "${version}" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "${description}" PARENT_SCOPE)
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

    # Every tag, filtered by the pattern.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} tag --list
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE all_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE list_result
    )

    # Tags that cannot be read leave the description unknown, rather than a
    # commit id that would pass for an unreleased tree.
    if(NOT list_result EQUAL 0)
        return()
    endif()

    # git separates names by newline, but a name may itself contain the CMake
    # list separator. Escaping it first keeps such a name in one piece: split
    # naively, v1.0.0-a;b becomes v1.0.0-a, which the pattern accepts and which
    # names no tag at all.
    string(REPLACE ";" "\;" all_tags "${all_tags}")
    string(REPLACE "\n" ";" all_tags "${all_tags}")
    set(selectors "")
    foreach(tag IN LISTS all_tags)
        # A name carrying the list separator cannot be handed to git as one
        # argument, so it cannot be selected. git permits the character; a
        # release tag has no reason to use it.
        if(tag MATCHES ";")
            continue()
        endif()

        scrap_version_from_tag(candidate "${tag}")
        if(NOT candidate STREQUAL "0.0.0")
            list(APPEND selectors "--match=${tag}")
        endif()
    endforeach()

    list(LENGTH selectors selector_count)
    if(selector_count EQUAL 0)
        # No release tag: the commit id is all there is to report. Asking
        # describe with no selector would let an unrelated tag answer, and an
        # empty --match is not documented behaviour to lean on.
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(NOT commit_description STREQUAL "")
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
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --always --dirty --abbrev=12 ${selectors}
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE describe_result
    )
    if(describe_result EQUAL 0 AND NOT describe STREQUAL "")
        set(${OUT_DESCRIBE} "${describe}" PARENT_SCOPE)
    else()
        scrap_describe_commit("${SOURCE_DIR}" commit_description)
        if(NOT commit_description STREQUAL "")
            set(${OUT_DESCRIBE} "${commit_description}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# Resolve the numeric version and the full git description of SOURCE_DIR: from
# the repository when it knows a release, from a filled-in archive file
# otherwise, and 0.0.0 with the repository's own description or an unknown one
# when neither knows more.
function(scrap_version_detect OUT_VERSION OUT_DESCRIBE SOURCE_DIR)
    scrap_version_from_repository("${SOURCE_DIR}" repository_found repository_version repository_describe)
    scrap_version_from_archive("${SOURCE_DIR}" archive_found archive_version archive_describe)

    if(repository_found AND NOT repository_version STREQUAL "0.0.0")
        set(detected_version "${repository_version}")
        set(detected_describe "${repository_describe}")
    elseif(archive_found)
        set(detected_version "${archive_version}")
        set(detected_describe "${archive_describe}")
    elseif(repository_found)
        set(detected_version "${repository_version}")
        set(detected_describe "${repository_describe}")
    else()
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
