# Derive the project version from git tags.
#
# Usage:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersion.cmake)
#   project_version_detect(<version-var> <describe-var>)
#   project(<name> VERSION ${<version-var>} ...)
#   project_version_banner(<banner-var> ${PROJECT_NAME} ${PROJECT_VERSION} ${<describe-var>})
#
# Release tags are named v<major>.<minor>.<patch> and may carry a pre-release
# suffix (v1.0.0-rc1). A tree with no such tag reports 0.0.0, which is what an
# unreleased checkout is. A tree without git, or an archive extracted without
# the repository, reports 0.0.0 and an unknown revision rather than failing:
# a source drop is still buildable, it just cannot say where it came from.
#
# Both values are read at configure time. Every entry point reconfigures
# (the Makefile targets and the workflows all run `cmake -S . -B`), and
# configure_file leaves the generated source untouched when the text is
# unchanged, so re-reading the tags on each configure costs nothing.

# Normalise a release tag to the major.minor.patch triple that
# project(VERSION) accepts. A pre-release suffix is dropped here and stays
# visible in the description. Anything that is not a release tag is 0.0.0.
function(project_version_from_tag OUT_VERSION TAG)
    if(TAG MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(${OUT_VERSION} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    endif()
endfunction()

# Resolve the numeric version and the full git description of the tree.
function(project_version_detect OUT_VERSION OUT_DESCRIBE)
    set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "unknown" PARENT_SCOPE)

    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        return()
    endif()

    # The nearest release tag.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0 --match=v[0-9]*
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE tag_result
    )
    if(tag_result EQUAL 0)
        project_version_from_tag(version "${tag}")
        set(${OUT_VERSION} "${version}" PARENT_SCOPE)
    endif()

    # The full description: pre-release suffix, commits since the tag, the
    # commit id, and whether the working tree carried uncommitted changes.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
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
function(project_version_banner OUT_BANNER NAME VERSION DESCRIBE)
    if(DESCRIBE STREQUAL "v${VERSION}")
        set(${OUT_BANNER} "${NAME} ${VERSION}" PARENT_SCOPE)
    else()
        set(${OUT_BANNER} "${NAME} ${VERSION} (${DESCRIBE})" PARENT_SCOPE)
    endif()
endfunction()
