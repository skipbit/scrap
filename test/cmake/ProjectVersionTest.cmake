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

# --- the tag pattern ---------------------------------------------------------

# A release tag carries the version.
expect_version("v0.1.0" "0.1.0")
expect_version("v1.2.3" "1.2.3")
expect_version("v10.20.30" "10.20.30")

# A pre-release resolves to the release it leads to; the suffix stays in the
# description reported alongside it.
expect_version("v0.2.0-rc1" "0.2.0")
expect_version("v1.0.0-0" "1.0.0")
expect_version("v1.0.0+build7" "1.0.0")

# Anything that is not a release tag leaves the tree unreleased. Without the
# terminating delimiter these would be truncated into versions they are not.
expect_version("0.1.0" "0.0.0")
expect_version("v0.1" "0.0.0")
expect_version("v1.2.3.4" "0.0.0")
expect_version("v0.1.0junk" "0.0.0")
expect_version("nightly" "0.0.0")
expect_version("" "0.0.0")

# Leading zeros are not accepted: CMake keeps them, so v01.2.3 would install
# as SOVERSION 01 and write "Version: 01.2.3" into dross.pc.
expect_version("v01.2.3" "0.0.0")
expect_version("v1.02.3" "0.0.0")
expect_version("v1.2.03" "0.0.0")

# A name carrying a CMake list separator is not a release tag, and must not
# become one by being split.
expect_version("v1;x.0.0" "0.0.0")

macro(expect_banner NAME VERSION DESCRIBE EXPECTED)
    scrap_version_banner(actual "${NAME}" "${VERSION}" "${DESCRIBE}")
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        record_failure("banner('${VERSION}', '${DESCRIBE}'): expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

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

macro(expect_preferred EXPECTED)
    scrap_version_preferred_tag(actual ${ARGN})
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        record_failure("preferred among '${ARGN}': expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

# --- which of several release tags on one commit names it --------------------

expect_preferred("v0.1.0" v0.1.0)
expect_preferred("" )

# The release beats its own pre-release and build metadata, in either order.
expect_preferred("v0.1.0" v0.1.0-rc1 v0.1.0)
expect_preferred("v0.1.0" v0.1.0 v0.1.0-rc1)
expect_preferred("v1.0.0" v1.0.0+build7 v1.0.0)

# The triple is compared as numbers: a name comparison would put 0.9 above 0.10.
expect_preferred("v0.10.0" v0.9.0 v0.10.0)

# Between two suffixed names the choice only has to be the same everywhere.
expect_preferred("v1.0.0-rc2" v1.0.0-rc1 v1.0.0-rc2)
expect_preferred("v1.0.0-rc2" v1.0.0-rc2 v1.0.0-rc1)

set(MINIMUM_CHECKS 30)

# --- detection against real repositories -------------------------------------

find_program(GIT_FOR_TEST NAMES git)
if(NOT GIT_FOR_TEST)
    message(STATUS "ProjectVersion: git not found, skipping the detection cases")
else()
    math(EXPR MINIMUM_CHECKS "${MINIMUM_CHECKS} + 30")

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

    # Tag names go through their own entry point: run_git passes ARGN, which
    # splits a name carrying a CMake list separator into two arguments.
    function(create_tag DIR TAG)
        execute_process(
            COMMAND ${GIT_FOR_TEST} -c user.email=t@example.invalid -c user.name=t tag "${TAG}"
            WORKING_DIRECTORY "${DIR}"
            RESULT_VARIABLE code
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "git tag '${TAG}' failed in ${DIR}")
        endif()
    endfunction()

    function(create_annotated_tag DIR TAG)
        execute_process(
            COMMAND ${GIT_FOR_TEST} -c user.email=t@example.invalid -c user.name=t tag -a "${TAG}" -m "${TAG}"
            WORKING_DIRECTORY "${DIR}"
            RESULT_VARIABLE code
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "git tag -a '${TAG}' failed in ${DIR}")
        endif()
    endfunction()

    # The project's own .gitattributes and ArchiveVersion.txt, so what is
    # exercised is the attribute and the file that ship, not a copy of their
    # intent.
    set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
    function(make_archivable_repo DIR)
        make_repo("${DIR}")
        file(COPY "${PROJECT_ROOT}/.gitattributes" DESTINATION "${DIR}")
        file(COPY "${PROJECT_ROOT}/cmake/ArchiveVersion.txt" DESTINATION "${DIR}/cmake")
        run_git("${DIR}" add .gitattributes cmake/ArchiveVersion.txt)
        run_git("${DIR}" commit -q -m archive)
    endfunction()

    # What GitHub attaches to a release: git archive of REF, unpacked.
    function(unpack_archive DIR REF DEST)
        file(REMOVE_RECURSE "${DEST}")
        file(MAKE_DIRECTORY "${DEST}")
        run_git("${DIR}" archive --format=tar "--output=${DEST}.tar" "${REF}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${DEST}.tar"
            WORKING_DIRECTORY "${DEST}"
            RESULT_VARIABLE code
        )
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "unpacking ${DEST}.tar failed")
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
    create_tag("${SCRATCH_DIR}/tagged" "v0.1.0")
    expect_detect("${SCRATCH_DIR}/tagged" "0.1.0" "^v0[.]1[.]0$" "on a release tag")

    # A commit past the release: the distance has to be measured from the
    # release, or the description names the wrong starting point.
    make_repo("${SCRATCH_DIR}/after")
    create_tag("${SCRATCH_DIR}/after" "v0.1.0")
    run_git("${SCRATCH_DIR}/after" commit -q --allow-empty -m next)
    expect_detect("${SCRATCH_DIR}/after" "0.1.0" "^v0[.]1[.]0-1-g[0-9a-f]+$" "one commit past a release")

    # A later tag that is not a release must not hide the release behind it.
    make_repo("${SCRATCH_DIR}/shadowed")
    create_tag("${SCRATCH_DIR}/shadowed" "v0.1.0")
    run_git("${SCRATCH_DIR}/shadowed" commit -q --allow-empty -m next)
    create_tag("${SCRATCH_DIR}/shadowed" "v0.2")
    expect_detect("${SCRATCH_DIR}/shadowed" "0.1.0" "^v0[.]1[.]0-1-g[0-9a-f]+$" "non-release tag ahead")

    # Tags no glob could filter out. They stack: the selection must not be
    # bounded by how many of them sit between HEAD and the release.
    make_repo("${SCRATCH_DIR}/malformed")
    create_tag("${SCRATCH_DIR}/malformed" "v0.1.0")
    run_git("${SCRATCH_DIR}/malformed" commit -q --allow-empty -m next)
    foreach(suffix RANGE 1 20)
        create_tag("${SCRATCH_DIR}/malformed" "v1.2.3.${suffix}")
    endforeach()
    create_tag("${SCRATCH_DIR}/malformed" "v01.2.3")
    create_tag("${SCRATCH_DIR}/malformed" "v0.1.0junk")
    expect_detect("${SCRATCH_DIR}/malformed" "0.1.0" "^v0[.]1[.]0-1-g[0-9a-f]+$" "malformed tags ahead")

    # A name carrying a CMake list separator cannot be handed to git as one
    # argument. The shape that bites is one whose leading fragment is itself
    # release-shaped: split naively it yields a selector naming no tag, and
    # the release behind it disappears.
    make_repo("${SCRATCH_DIR}/separator")
    create_tag("${SCRATCH_DIR}/separator" "v0.1.0")
    run_git("${SCRATCH_DIR}/separator" commit -q --allow-empty -m next)
    create_tag("${SCRATCH_DIR}/separator" "v1.0.0-a;b")
    expect_detect("${SCRATCH_DIR}/separator" "0.1.0" "^v0[.]1[.]0-1-g[0-9a-f]+$" "release-shaped name with a list separator")

    # A name that only git would allow reaches a C++ string literal through
    # the generated version source. An unescaped quote would break the
    # translation unit, or carry what follows it into the source.
    make_repo("${SCRATCH_DIR}/quoted")
    create_tag("${SCRATCH_DIR}/quoted" "v1.0.0-q\"x")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -D "SOURCE_DIR=${SCRATCH_DIR}/quoted"
            -D "NAME=scrap"
            -D "INPUT=${CMAKE_CURRENT_LIST_DIR}/../../src/shared/constants/version.cpp.in"
            -D "OUTPUT=${SCRATCH_DIR}/quoted.cpp"
            -P "${CMAKE_CURRENT_LIST_DIR}/../../cmake/GenerateVersionSource.cmake"
        RESULT_VARIABLE generate_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    math(EXPR checks "${checks} + 1")
    if(NOT generate_result EQUAL 0)
        record_failure("generating the version source failed for a quoted tag name")
    else()
        file(READ "${SCRATCH_DIR}/quoted.cpp" generated)
        math(EXPR checks "${checks} + 1")
        # The quote has to arrive escaped, and the literal has to stay closed.
        if(NOT generated MATCHES "return \"[^\n]*q\\\\\"x[^\n]*\";")
            record_failure("the generated source does not escape the quote: ${generated}")
        endif()
    endif()

    # Tags that are not releases at all leave the tree unreleased, described
    # by its commit id rather than by one of them.
    make_repo("${SCRATCH_DIR}/nonrelease")
    create_tag("${SCRATCH_DIR}/nonrelease" "nightly-2026")
    create_tag("${SCRATCH_DIR}/nonrelease" "v0.2")
    expect_detect("${SCRATCH_DIR}/nonrelease" "0.0.0" "^[0-9a-f]+$" "only non-release tags")

    # A non-release tag sharing the release's commit must not become the
    # description.
    make_repo("${SCRATCH_DIR}/colocated")
    create_tag("${SCRATCH_DIR}/colocated" "v0.1.0")
    create_tag("${SCRATCH_DIR}/colocated" "nightly-2026")
    expect_detect("${SCRATCH_DIR}/colocated" "0.1.0" "^v0[.]1[.]0$" "non-release tag on the same commit")

    # A pre-release promoted to its release on the same commit. git names the
    # commit by the annotated tag; the rule names it by the release, and the
    # archive below has to agree.
    make_archivable_repo("${SCRATCH_DIR}/promoted")
    create_annotated_tag("${SCRATCH_DIR}/promoted" "v0.2.0-rc1")
    create_tag("${SCRATCH_DIR}/promoted" "v0.2.0")
    create_tag("${SCRATCH_DIR}/promoted" "nightly-2026")
    create_tag("${SCRATCH_DIR}/promoted" "v1.2.3.4")
    expect_detect("${SCRATCH_DIR}/promoted" "0.2.0" "^v0[.]2[.]0$" "promoted pre-release, checkout")

    # The release tarball: no repository, only what git archive filled in.
    unpack_archive("${SCRATCH_DIR}/promoted" "v0.2.0" "${SCRATCH_DIR}/promoted-archive")
    expect_detect("${SCRATCH_DIR}/promoted-archive" "0.2.0" "^v0[.]2[.]0$" "release archive")

    # An archive made between releases knows only its own commit.
    run_git("${SCRATCH_DIR}/promoted" commit -q --allow-empty -m next)
    expect_detect("${SCRATCH_DIR}/promoted" "0.2.0" "^v0[.]2[.]0-1-g[0-9a-f]+$" "promoted pre-release, one commit past")
    unpack_archive("${SCRATCH_DIR}/promoted" "HEAD" "${SCRATCH_DIR}/between-archive")
    expect_detect("${SCRATCH_DIR}/between-archive" "0.0.0" "^[0-9a-f]+$" "archive between releases")

    # The file as it sits in a checkout, copied without git: its placeholders
    # were never filled in, and must not be read as a commit or a tag.
    file(MAKE_DIRECTORY "${SCRATCH_DIR}/unsubstituted/cmake")
    file(COPY "${PROJECT_ROOT}/cmake/ArchiveVersion.txt" DESTINATION "${SCRATCH_DIR}/unsubstituted/cmake")
    expect_detect("${SCRATCH_DIR}/unsubstituted" "0.0.0" "^unknown$" "unsubstituted archive file")

    # Sources unpacked inside another project: git finds that project's
    # repository, whose tags say nothing about this one.
    make_repo("${SCRATCH_DIR}/host")
    create_tag("${SCRATCH_DIR}/host" "v9.9.9")
    file(MAKE_DIRECTORY "${SCRATCH_DIR}/host/vendor/scrap")
    expect_detect("${SCRATCH_DIR}/host/vendor/scrap" "0.0.0" "^unknown$" "sources inside another repository")

    file(REMOVE_RECURSE "${SCRATCH_DIR}")
endif()

# --- the gate itself ---------------------------------------------------------

# An empty run exits 0 without this: every assertion could be removed and the
# test would still report success.
if(checks LESS MINIMUM_CHECKS)
    message(SEND_ERROR "ProjectVersion: ran ${checks} checks, expected at least ${MINIMUM_CHECKS}")
elseif(failures GREATER 0)
    message(STATUS "ProjectVersion: ${failures} of ${checks} checks failed")
else()
    message(STATUS "ProjectVersion: ${checks} checks passed")
endif()
