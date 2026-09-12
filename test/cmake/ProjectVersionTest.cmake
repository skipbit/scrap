# Tests for cmake/ProjectVersion.cmake.
#
# The version is resolved before any C++ exists, so the logic is exercised by
# running the module in script mode (cmake -P). ctest registers this file; see
# test/CMakeLists.txt.
#
# scrap_version_detect is covered against real repositories built here. It is
# where the tag selection, the working directory and the fallbacks live, and a
# defect in it reaches the binary without anything else noticing: the e2e
# checks only see whatever the build produced.
#
# SCRATCH_DIR is where the temporary repositories go; the caller passes it.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ProjectVersion.cmake")

if(NOT SCRATCH_DIR)
    set(SCRATCH_DIR "$ENV{TMPDIR}")
    if(NOT SCRATCH_DIR)
        set(SCRATCH_DIR "/tmp")
    endif()
    set(SCRATCH_DIR "${SCRATCH_DIR}/scrap-version-test")
endif()

set(checks 0)
set(failures 0)

macro(record_failure MESSAGE)
    math(EXPR failures "${failures} + 1")
    message(SEND_ERROR "${MESSAGE}")
endmacro()

macro(expect_version TAG EXPECTED)
    scrap_version_from_tag(actual "${TAG}")
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        record_failure("tag '${TAG}': expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

macro(expect_banner NAME VERSION DESCRIBE EXPECTED)
    scrap_version_banner(actual "${NAME}" "${VERSION}" "${DESCRIBE}")
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        record_failure("banner('${VERSION}', '${DESCRIBE}'): expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

# --- the tag pattern ---------------------------------------------------------

# A release tag carries the version.
expect_version("v0.1.0" "0.1.0")
expect_version("v1.2.3" "1.2.3")
expect_version("v10.20.30" "10.20.30")

# A pre-release resolves to the release it leads to; the suffix stays in the
# description, which is where the banner shows it.
expect_version("v0.2.0-rc1" "0.2.0")
expect_version("v1.0.0+build7" "1.0.0")

# Anything that is not a release tag leaves the tree unreleased. Without the
# terminating delimiter these would be truncated into versions they are not.
expect_version("0.1.0" "0.0.0")
expect_version("v0.1" "0.0.0")
expect_version("v1.2.3.4" "0.0.0")
expect_version("v0.1.0junk" "0.0.0")
expect_version("nightly" "0.0.0")
expect_version("" "0.0.0")

# --- the banner --------------------------------------------------------------

# A build standing exactly on a release tag says only the release.
expect_banner("scrap" "0.1.0" "v0.1.0" "scrap 0.1.0")

# Anything else carries the description, so the binary can be traced back to
# the commit it came from.
expect_banner("scrap" "0.1.0" "v0.1.0-2-g2a761c0" "scrap 0.1.0 (v0.1.0-2-g2a761c0)")
expect_banner("scrap" "0.1.0" "v0.1.0-2-g2a761c0-dirty" "scrap 0.1.0 (v0.1.0-2-g2a761c0-dirty)")
expect_banner("scrap" "0.0.0" "5044354" "scrap 0.0.0 (5044354)")
expect_banner("scrap" "0.0.0" "unknown" "scrap 0.0.0 (unknown)")

# A pre-release is not the release tag it leads to, so it stays visible.
expect_banner("scrap" "0.2.0" "v0.2.0-rc1" "scrap 0.2.0 (v0.2.0-rc1)")

# --- detection against real repositories -------------------------------------

find_program(GIT_FOR_TEST NAMES git)
if(NOT GIT_FOR_TEST)
    message(STATUS "ProjectVersion: git not found, skipping the detection cases")
else()
    function(run_git DIR)
        execute_process(
            COMMAND ${GIT_FOR_TEST} -c user.email=t@example.invalid -c user.name=t ${ARGN}
            WORKING_DIRECTORY "${DIR}"
            RESULT_VARIABLE code
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "git ${ARGN} failed in ${DIR}")
        endif()
    endfunction()

    function(make_repo DIR)
        file(REMOVE_RECURSE "${DIR}")
        file(MAKE_DIRECTORY "${DIR}")
        run_git("${DIR}" init -q)
        run_git("${DIR}" commit -q --allow-empty -m init)
    endfunction()

    macro(expect_detect DIR EXPECTED_VERSION DESCRIBE_PATTERN LABEL)
        scrap_version_detect(got_version got_describe "${DIR}")
        math(EXPR checks "${checks} + 1")
        if(NOT got_version STREQUAL "${EXPECTED_VERSION}")
            record_failure("${LABEL}: version expected '${EXPECTED_VERSION}', got '${got_version}'")
        endif()
        math(EXPR checks "${checks} + 1")
        if(NOT got_describe MATCHES "${DESCRIBE_PATTERN}")
            record_failure("${LABEL}: describe '${got_describe}' does not match '${DESCRIBE_PATTERN}'")
        endif()
    endmacro()

    file(REMOVE_RECURSE "${SCRATCH_DIR}")

    # No tag at all: an unreleased tree, described by its commit id.
    make_repo("${SCRATCH_DIR}/untagged")
    expect_detect("${SCRATCH_DIR}/untagged" "0.0.0" "^[0-9a-f]+$" "untagged")

    # Standing on a release tag.
    make_repo("${SCRATCH_DIR}/tagged")
    run_git("${SCRATCH_DIR}/tagged" tag v0.1.0)
    expect_detect("${SCRATCH_DIR}/tagged" "0.1.0" "^v0[.]1[.]0$" "on a release tag")

    # A commit past the release: the distance has to be measured from the
    # release, which is what makes the banner show it.
    make_repo("${SCRATCH_DIR}/after")
    run_git("${SCRATCH_DIR}/after" tag v0.1.0)
    run_git("${SCRATCH_DIR}/after" commit -q --allow-empty -m next)
    expect_detect("${SCRATCH_DIR}/after" "0.1.0" "^v0[.]1[.]0-1-g[0-9a-f]+$" "one commit past a release")

    # A later tag that is not a release must not hide the release behind it.
    make_repo("${SCRATCH_DIR}/shadowed")
    run_git("${SCRATCH_DIR}/shadowed" tag v0.1.0)
    run_git("${SCRATCH_DIR}/shadowed" commit -q --allow-empty -m next)
    run_git("${SCRATCH_DIR}/shadowed" tag v0.2)
    expect_detect("${SCRATCH_DIR}/shadowed" "0.1.0" "^v0[.]1[.]0-1-g[0-9a-f]+$" "non-release tag ahead")

    # A non-release tag sharing the release's commit must not become the
    # description, or a release build stops looking like one.
    make_repo("${SCRATCH_DIR}/colocated")
    run_git("${SCRATCH_DIR}/colocated" tag v0.1.0)
    run_git("${SCRATCH_DIR}/colocated" tag nightly-2026)
    expect_detect("${SCRATCH_DIR}/colocated" "0.1.0" "^v0[.]1[.]0$" "non-release tag on the same commit")

    # Sources unpacked inside another project: git finds that project's
    # repository, whose tags say nothing about this one.
    make_repo("${SCRATCH_DIR}/host")
    run_git("${SCRATCH_DIR}/host" tag v9.9.9)
    file(MAKE_DIRECTORY "${SCRATCH_DIR}/host/vendor/scrap")
    expect_detect("${SCRATCH_DIR}/host/vendor/scrap" "0.0.0" "^unknown$" "sources inside another repository")

    file(REMOVE_RECURSE "${SCRATCH_DIR}")
endif()

# --- the gate itself ---------------------------------------------------------

# An empty run exits 0 without this: every assertion could be removed and the
# test would still report success.
set(MINIMUM_CHECKS 17)
if(checks LESS MINIMUM_CHECKS)
    message(SEND_ERROR "ProjectVersion: ran ${checks} checks, expected at least ${MINIMUM_CHECKS}")
elseif(failures GREATER 0)
    message(STATUS "ProjectVersion: ${failures} of ${checks} checks failed")
else()
    message(STATUS "ProjectVersion: ${checks} checks passed")
endif()
