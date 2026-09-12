# Derive the project version from git tags.
#
# Usage:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersion.cmake)
#   scrap_version_detect(<version-var> <describe-var> <source-dir>)
#   project(<name> VERSION ${<version-var>} ...)
#   scrap_version_banner(<banner-var> <name> <version> <describe>)
#
# Release tags are named v<major>.<minor>.<patch> and may carry a pre-release
# suffix (v1.0.0-rc1). Only tags of that shape count: v0.2, v1.2.3.4 and
# nightly-2026 are not releases, and do not become releases by sitting closer
# to HEAD than one.
#
# A tree with no release tag reports 0.0.0, which is what an unreleased
# checkout is. So does a tree that is not this project's own repository --
# git searches upwards, so sources unpacked inside another project would
# otherwise report that project's version as ours.
#
# Both values are read at configure time, which is the only point where
# project(VERSION) can take them. The generated version source is refreshed
# on every build instead (see cmake/GenerateVersionSource.cmake), so what the
# binary prints follows the tags without waiting for a reconfigure.

# The glob git filters candidate tags with. It has to be at least as strict
# as the pattern below, or a tag that is rejected here can still hide a real
# release from `describe --abbrev=0`.
set(SCRAP_RELEASE_TAG_GLOB "v[0-9]*.[0-9]*.[0-9]*")

# Normalise a release tag to the major.minor.patch triple that
# project(VERSION) accepts. A pre-release suffix is dropped here and stays
# visible in the description. Anything else is 0.0.0.
function(scrap_version_from_tag OUT_VERSION TAG)
    if(TAG MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)($|[-+])")
        set(${OUT_VERSION} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    endif()
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
    # tree, so without this an archive extracted inside another checkout
    # would inherit that checkout's tags.
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

    # The nearest release tag.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --abbrev=0 --match=${SCRAP_RELEASE_TAG_GLOB}
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE tag_result
    )
    if(tag_result EQUAL 0)
        scrap_version_from_tag(version "${tag}")
        if(version STREQUAL "0.0.0")
            # The glob let a tag through that the pattern rejects. Saying so
            # beats reporting an unreleased tree while a tag is checked out.
            message(WARNING "ignoring tag '${tag}': not a release tag")
        endif()
        set(${OUT_VERSION} "${version}" PARENT_SCOPE)
    endif()

    # The full description, filtered by the same glob so it describes distance
    # from a release rather than from whatever tag happens to be nearest.
    execute_process(
        COMMAND ${SCRAP_GIT_EXECUTABLE} describe --tags --always --dirty --match=${SCRAP_RELEASE_TAG_GLOB}
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE describe_result
    )
    if(describe_result EQUAL 0 AND describe)
        set(${OUT_DESCRIBE} "${describe}" PARENT_SCOPE)
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
