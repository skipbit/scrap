# Derives the project version from git tags.
#
#   include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersion.cmake)
#   scrap_version_detect(<version-var> <describe-var> <source-dir>)
#   project(<name> VERSION ${<version-var>} ...)
#
# A release tag is v<major>.<minor>.<patch> without leading zeros, optionally
# followed by -<pre-release> or +<build>. Other tags are ignored.
#
# In the project's own repository, the version comes from the nearest release
# tag. Tags are filtered by the pattern above rather than by a glob, so a
# malformed tag cannot hide a release.
#
# Any other tree is read from cmake/ArchiveVersion.txt, which git archive fills
# in with the commit and its description. The file keeps an archive's bytes
# fixed as branches move; they change when a release tag is later added to the
# archived commit or to one of its ancestors. The description is selected by
# a glob that also admits malformed tags such as v1.2.3.4, and when git picks
# one the archive reports 0.0.0.
#
# A tree with neither reports 0.0.0 and "unknown".
#
# Commit ids are abbreviated to 12 characters. Both values are read at
# configure time; cmake/GenerateVersionSource.cmake refreshes the banner on
# every build. Strings are compared with STREQUAL because CMake treats a value
# ending in -NOTFOUND as false.

# Returns the major.minor.patch of a release tag, or 0.0.0.
function(scrap_version_from_tag OUT_VERSION TAG)
    set(component "(0|[1-9][0-9]*)")
    if(TAG MATCHES "^v${component}\\.${component}\\.${component}($|[-+])")
        set(${OUT_VERSION} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    else()
        set(${OUT_VERSION} "0.0.0" PARENT_SCOPE)
    endif()
endfunction()

# Describes a commit without a release tag: its id, with -dirty for
# uncommitted changes.
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

# Reads the repository whose root is SOURCE_DIR. OUT_FOUND is TRUE when there
# is one.
function(scrap_version_from_repository SOURCE_DIR OUT_FOUND OUT_VERSION OUT_DESCRIBE)
    set(${OUT_FOUND} FALSE PARENT_SCOPE)

    # A plain program lookup: find_package(Git) before project() would run
    # without the toolchain's search settings.
    find_program(SCRAP_GIT_EXECUTABLE NAMES git)
    if(NOT SCRAP_GIT_EXECUTABLE)
        return()
    endif()

    # git searches upwards; only a repository rooted here belongs to this tree.
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
    if(NOT list_result EQUAL 0)
        return()
    endif()

    # Escaping keeps a name containing ';' whole, so it can be skipped: git
    # cannot receive it as one argument.
    string(REPLACE ";" "\;" all_tags "${all_tags}")
    string(REPLACE "\n" ";" all_tags "${all_tags}")
    set(selectors "")
    foreach(tag IN LISTS all_tags)
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

# Reads cmake/ArchiveVersion.txt under SOURCE_DIR. OUT_FOUND is TRUE when git
# archive filled it in.
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

    # The release pattern accepts a description such as v0.1.0-3-g..., and
    # rejects an empty value, an unexpanded placeholder and a malformed tag.
    # Names containing ';' are rejected as in a repository.
    if(description MATCHES ";")
        return()
    endif()
    scrap_version_from_tag(version "${description}")
    if(version STREQUAL "0.0.0")
        return()
    endif()

    set(${OUT_VERSION} "${version}" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "${description}" PARENT_SCOPE)
endfunction()

# Resolves the version and description of SOURCE_DIR.
function(scrap_version_detect OUT_VERSION OUT_DESCRIBE SOURCE_DIR)
    set(detected_version "0.0.0")
    set(detected_describe "unknown")

    scrap_version_from_repository("${SOURCE_DIR}" found found_version found_describe)
    if(NOT found)
        scrap_version_from_archive("${SOURCE_DIR}" found found_version found_describe)
    endif()
    if(found)
        set(detected_version "${found_version}")
        set(detected_describe "${found_describe}")
    endif()

    set(${OUT_VERSION} "${detected_version}" PARENT_SCOPE)
    set(${OUT_DESCRIBE} "${detected_describe}" PARENT_SCOPE)
endfunction()

# Composes the banner: the release alone on a release tag, otherwise the
# release and the description.
function(scrap_version_banner OUT_BANNER NAME VERSION DESCRIBE)
    if(DESCRIBE STREQUAL "v${VERSION}")
        set(${OUT_BANNER} "${NAME} ${VERSION}" PARENT_SCOPE)
    else()
        set(${OUT_BANNER} "${NAME} ${VERSION} (${DESCRIBE})" PARENT_SCOPE)
    endif()
endfunction()
