# Tests for cmake/ProjectVersion.cmake, run by ctest through cmake -P.
#
# The detection cases build real repositories and archives under SCRATCH_DIR,
# which the caller passes.

cmake_minimum_required(VERSION 3.20)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${PROJECT_ROOT}/cmake/ProjectVersion.cmake")

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

expect_version("v0.1.0" "0.1.0")
expect_version("v1.2.3" "1.2.3")
expect_version("v10.20.30" "10.20.30")

expect_version("v0.2.0-rc1" "0.2.0")
expect_version("v1.0.0-0" "1.0.0")
expect_version("v1.0.0+build7" "1.0.0")

expect_version("0.1.0" "0.0.0")
expect_version("v0.1" "0.0.0")
expect_version("v1.2.3.4" "0.0.0")
expect_version("v0.1.0junk" "0.0.0")
expect_version("nightly" "0.0.0")
expect_version("" "0.0.0")

expect_version("v01.2.3" "0.0.0")
expect_version("v1.02.3" "0.0.0")
expect_version("v1.2.03" "0.0.0")

expect_version("v1;x.0.0" "0.0.0")
expect_version("v1.0.0-NOTFOUND" "1.0.0")

# --- the banner --------------------------------------------------------------

macro(expect_banner NAME VERSION DESCRIBE EXPECTED)
    scrap_version_banner(actual "${NAME}" "${VERSION}" "${DESCRIBE}")
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        record_failure("banner('${VERSION}', '${DESCRIBE}'): expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

expect_banner("scrap" "0.1.0" "v0.1.0" "scrap 0.1.0")
expect_banner("scrap" "0.1.0" "v0.1.0-2-g2a761c0" "scrap 0.1.0 (v0.1.0-2-g2a761c0)")
expect_banner("scrap" "0.1.0" "v0.1.0-2-g2a761c0-dirty" "scrap 0.1.0 (v0.1.0-2-g2a761c0-dirty)")
expect_banner("scrap" "0.0.0" "5044354" "scrap 0.0.0 (5044354)")
expect_banner("scrap" "0.0.0" "unknown" "scrap 0.0.0 (unknown)")
expect_banner("scrap" "0.2.0" "v0.2.0-rc1" "scrap 0.2.0 (v0.2.0-rc1)")

# --- the archive template ----------------------------------------------------

# The placeholders the archive cases use. In a checkout they are compared with
# the shipped file; in an unpacked archive the shipped file is filled in.
set(ARCHIVE_TEMPLATE "commit=$Format:%H$\ndescribe=$Format:%(describe:tags=true,abbrev=12,match=v[0-9]*.[0-9]*.[0-9]*,exclude=*;*)$\n")
file(READ "${PROJECT_ROOT}/cmake/ArchiveVersion.txt" shipped_archive_file)
string(REGEX MATCHALL "(commit|describe)=[^\n]*" shipped_lines "${shipped_archive_file}")
string(REGEX MATCHALL "(commit|describe)=[^\n]*" template_lines "${ARCHIVE_TEMPLATE}")
math(EXPR checks "${checks} + 1")
if(shipped_archive_file MATCHES "[$]Format:")
    if(NOT shipped_lines STREQUAL template_lines)
        record_failure("the placeholders in this test differ from cmake/ArchiveVersion.txt")
    endif()
elseif(NOT (shipped_archive_file MATCHES "(^|\n)commit=[0-9a-f]+" AND shipped_archive_file MATCHES "(^|\n)describe="))
    record_failure("cmake/ArchiveVersion.txt is neither a template nor filled in: ${shipped_archive_file}")
endif()

set(MINIMUM_CHECKS 24)

# --- detection ---------------------------------------------------------------

find_program(GIT_FOR_TEST NAMES git)
if(NOT GIT_FOR_TEST)
    message(STATUS "ProjectVersion: git not found, skipping the detection cases")
else()
    math(EXPR MINIMUM_CHECKS "${MINIMUM_CHECKS} + 60")
    string(REPEAT "[0-9a-f]" 12 HEX12)
    set(GIT_FLAGS -c user.email=t@example.invalid -c user.name=t -c commit.gpgSign=false -c tag.gpgSign=false)

    function(run_git DIR)
        execute_process(
            COMMAND ${GIT_FOR_TEST} ${GIT_FLAGS} ${ARGN}
            WORKING_DIRECTORY "${DIR}"
            RESULT_VARIABLE code
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "git ${ARGN} failed in ${DIR}")
        endif()
    endfunction()

    # The name is quoted apart from ARGN, which would split a name containing
    # ';'. Options such as -a -m go in ARGN.
    function(create_tag DIR TAG)
        execute_process(
            COMMAND ${GIT_FOR_TEST} ${GIT_FLAGS} tag ${ARGN} "${TAG}"
            WORKING_DIRECTORY "${DIR}"
            RESULT_VARIABLE code
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "git tag '${TAG}' failed in ${DIR}")
        endif()
    endfunction()

    function(make_repo DIR)
        file(REMOVE_RECURSE "${DIR}")
        file(MAKE_DIRECTORY "${DIR}")
        run_git("${DIR}" init -q)
        run_git("${DIR}" commit -q --allow-empty -m init)
    endfunction()

    function(make_archivable_repo DIR)
        make_repo("${DIR}")
        file(COPY "${PROJECT_ROOT}/.gitattributes" DESTINATION "${DIR}")
        file(WRITE "${DIR}/cmake/ArchiveVersion.txt" "${ARCHIVE_TEMPLATE}")
        run_git("${DIR}" add .gitattributes cmake/ArchiveVersion.txt)
        run_git("${DIR}" commit -q -m archive)
    endfunction()

    # Unpacks git archive of REF, as GitHub attaches it to a release.
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

    # Compares the last detection with another tree's result.
    macro(expect_same LABEL VERSION DESCRIBE)
        math(EXPR checks "${checks} + 1")
        if(NOT got_version STREQUAL "${VERSION}")
            record_failure("${LABEL}: version ${got_version}, the other tree ${VERSION}")
        endif()
        math(EXPR checks "${checks} + 1")
        if(NOT got_describe STREQUAL "${DESCRIBE}")
            record_failure("${LABEL}: description '${got_describe}', the other tree '${DESCRIBE}'")
        endif()
    endmacro()

    file(REMOVE_RECURSE "${SCRATCH_DIR}")

    # --- checkouts ---

    make_repo("${SCRATCH_DIR}/untagged")
    expect_detect("${SCRATCH_DIR}/untagged" "0.0.0" "^${HEX12}$" "untagged")

    make_repo("${SCRATCH_DIR}/tagged")
    create_tag("${SCRATCH_DIR}/tagged" "v0.1.0")
    expect_detect("${SCRATCH_DIR}/tagged" "0.1.0" "^v0[.]1[.]0$" "on a release tag")

    make_repo("${SCRATCH_DIR}/after")
    create_tag("${SCRATCH_DIR}/after" "v0.1.0")
    run_git("${SCRATCH_DIR}/after" commit -q --allow-empty -m next)
    expect_detect("${SCRATCH_DIR}/after" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "one commit past a release")

    make_repo("${SCRATCH_DIR}/shadowed")
    create_tag("${SCRATCH_DIR}/shadowed" "v0.1.0")
    run_git("${SCRATCH_DIR}/shadowed" commit -q --allow-empty -m next)
    create_tag("${SCRATCH_DIR}/shadowed" "v0.2")
    expect_detect("${SCRATCH_DIR}/shadowed" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "non-release tag ahead")

    # Malformed tags a glob would admit, stacked ahead of the release.
    make_repo("${SCRATCH_DIR}/malformed")
    create_tag("${SCRATCH_DIR}/malformed" "v0.1.0")
    run_git("${SCRATCH_DIR}/malformed" commit -q --allow-empty -m next)
    foreach(suffix RANGE 1 20)
        create_tag("${SCRATCH_DIR}/malformed" "v1.2.3.${suffix}")
    endforeach()
    create_tag("${SCRATCH_DIR}/malformed" "v01.2.3")
    create_tag("${SCRATCH_DIR}/malformed" "v0.1.0junk")
    expect_detect("${SCRATCH_DIR}/malformed" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "malformed tags ahead")

    make_repo("${SCRATCH_DIR}/separator")
    create_tag("${SCRATCH_DIR}/separator" "v0.1.0")
    run_git("${SCRATCH_DIR}/separator" commit -q --allow-empty -m next)
    create_tag("${SCRATCH_DIR}/separator" "v1.0.0-a;b")
    expect_detect("${SCRATCH_DIR}/separator" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "release-shaped name with ';'")

    # A tag name reaches a C++ string literal through the generated source.
    make_repo("${SCRATCH_DIR}/quoted")
    create_tag("${SCRATCH_DIR}/quoted" "v1.0.0-q\"x")
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -D "SOURCE_DIR=${SCRATCH_DIR}/quoted"
            -D "NAME=scrap"
            -D "INPUT=${PROJECT_ROOT}/src/shared/constants/version.cpp.in"
            -D "OUTPUT=${SCRATCH_DIR}/quoted.cpp"
            -P "${PROJECT_ROOT}/cmake/GenerateVersionSource.cmake"
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
        if(NOT generated MATCHES "return \"[^\n]*q\\\\\"x[^\n]*\";")
            record_failure("the generated source does not escape the quote: ${generated}")
        endif()
    endif()

    make_repo("${SCRATCH_DIR}/nonrelease")
    create_tag("${SCRATCH_DIR}/nonrelease" "nightly-2026")
    create_tag("${SCRATCH_DIR}/nonrelease" "v0.2")
    expect_detect("${SCRATCH_DIR}/nonrelease" "0.0.0" "^${HEX12}$" "only non-release tags")

    make_repo("${SCRATCH_DIR}/colocated")
    create_tag("${SCRATCH_DIR}/colocated" "v0.1.0")
    create_tag("${SCRATCH_DIR}/colocated" "nightly-2026")
    expect_detect("${SCRATCH_DIR}/colocated" "0.1.0" "^v0[.]1[.]0$" "non-release tag on the release commit")

    # Sources inside another project's repository.
    make_repo("${SCRATCH_DIR}/host")
    create_tag("${SCRATCH_DIR}/host" "v9.9.9")
    file(MAKE_DIRECTORY "${SCRATCH_DIR}/host/vendor/scrap")
    expect_detect("${SCRATCH_DIR}/host/vendor/scrap" "0.0.0" "^unknown$" "sources inside another repository")

    # --- archives ---

    make_archivable_repo("${SCRATCH_DIR}/archived")
    create_tag("${SCRATCH_DIR}/archived" "v0.1.0" -a -m "v0.1.0")
    create_tag("${SCRATCH_DIR}/archived" "nightly-2026")
    unpack_archive("${SCRATCH_DIR}/archived" "v0.1.0" "${SCRATCH_DIR}/release-archive")
    expect_detect("${SCRATCH_DIR}/release-archive" "0.1.0" "^v0[.]1[.]0$" "release archive")

    run_git("${SCRATCH_DIR}/archived" commit -q --allow-empty -m next)
    scrap_version_detect(checkout_version checkout_describe "${SCRATCH_DIR}/archived")
    unpack_archive("${SCRATCH_DIR}/archived" "HEAD" "${SCRATCH_DIR}/between-archive")
    expect_detect("${SCRATCH_DIR}/between-archive" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "archive between releases")
    expect_same("archive between releases" "${checkout_version}" "${checkout_describe}")

    make_archivable_repo("${SCRATCH_DIR}/unreleased")
    unpack_archive("${SCRATCH_DIR}/unreleased" "HEAD" "${SCRATCH_DIR}/unreleased-archive")
    expect_detect("${SCRATCH_DIR}/unreleased-archive" "0.0.0" "^${HEX12}$" "archive with no release")

    # A limit: a malformed tag nearer than the release leaves the archive
    # without a version.
    make_archivable_repo("${SCRATCH_DIR}/archive-malformed")
    create_tag("${SCRATCH_DIR}/archive-malformed" "v0.1.0" -a -m "v0.1.0")
    run_git("${SCRATCH_DIR}/archive-malformed" commit -q --allow-empty -m next)
    create_tag("${SCRATCH_DIR}/archive-malformed" "v1.2.3.4" -a -m "v1.2.3.4")
    unpack_archive("${SCRATCH_DIR}/archive-malformed" "HEAD" "${SCRATCH_DIR}/archive-malformed-archive")
    expect_detect("${SCRATCH_DIR}/archive-malformed-archive" "0.0.0" "^${HEX12}$" "archive with a malformed tag nearest")

    # git chooses between a pre-release and its release on one commit, and the
    # checkout and the archive agree on the choice.
    make_archivable_repo("${SCRATCH_DIR}/promoted")
    create_tag("${SCRATCH_DIR}/promoted" "v0.2.0-rc1" -a -m "v0.2.0-rc1")
    create_tag("${SCRATCH_DIR}/promoted" "v0.2.0")
    scrap_version_detect(promoted_version promoted_describe "${SCRATCH_DIR}/promoted")
    unpack_archive("${SCRATCH_DIR}/promoted" "v0.2.0" "${SCRATCH_DIR}/promoted-archive")
    expect_detect("${SCRATCH_DIR}/promoted-archive" "0.2.0" "^v0[.]2[.]0" "promoted pre-release, archive")
    expect_same("promoted pre-release" "${promoted_version}" "${promoted_describe}")

    # An archive made into a repository describes that repository.
    unpack_archive("${SCRATCH_DIR}/archived" "v0.1.0" "${SCRATCH_DIR}/reinitialised")
    run_git("${SCRATCH_DIR}/reinitialised" init -q)
    run_git("${SCRATCH_DIR}/reinitialised" add -A)
    run_git("${SCRATCH_DIR}/reinitialised" commit -q -m import)
    execute_process(
        COMMAND ${GIT_FOR_TEST} rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${SCRATCH_DIR}/reinitialised"
        OUTPUT_VARIABLE reinitialised_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    expect_detect("${SCRATCH_DIR}/reinitialised" "0.0.0" "^${reinitialised_head}$" "archive made into a repository")

    run_git("${SCRATCH_DIR}/reinitialised" commit -q --allow-empty -m work)
    create_tag("${SCRATCH_DIR}/reinitialised" "v0.2.0" -a -m "v0.2.0")
    expect_detect("${SCRATCH_DIR}/reinitialised" "0.2.0" "^v0[.]2[.]0$" "archive made into a repository, released since")

    file(WRITE "${SCRATCH_DIR}/unexpanded/cmake/ArchiveVersion.txt"
        "commit=0123456789abcdef0123456789abcdef01234567\ndescribe=%(describe:tags=true,abbrev=12,match=v[0-9]*.[0-9]*.[0-9]*,exclude=*;*)\n")
    expect_detect("${SCRATCH_DIR}/unexpanded" "0.0.0" "^0123456789ab$" "describe placeholder left unexpanded")

    file(WRITE "${SCRATCH_DIR}/unsubstituted/cmake/ArchiveVersion.txt" "${ARCHIVE_TEMPLATE}")
    expect_detect("${SCRATCH_DIR}/unsubstituted" "0.0.0" "^unknown$" "unsubstituted archive file")

    make_archivable_repo("${SCRATCH_DIR}/notfound")
    create_tag("${SCRATCH_DIR}/notfound" "v1.0.0-NOTFOUND")
    expect_detect("${SCRATCH_DIR}/notfound" "1.0.0" "^v1[.]0[.]0-NOTFOUND$" "tag ending in -NOTFOUND, checkout")
    unpack_archive("${SCRATCH_DIR}/notfound" "v1.0.0-NOTFOUND" "${SCRATCH_DIR}/notfound-archive")
    expect_detect("${SCRATCH_DIR}/notfound-archive" "1.0.0" "^v1[.]0[.]0-NOTFOUND$" "tag ending in -NOTFOUND, archive")

    make_archivable_repo("${SCRATCH_DIR}/archive-separator")
    create_tag("${SCRATCH_DIR}/archive-separator" "v0.1.0" -a -m "v0.1.0")
    run_git("${SCRATCH_DIR}/archive-separator" commit -q --allow-empty -m next)
    create_tag("${SCRATCH_DIR}/archive-separator" "v1.0.0-a;b")
    expect_detect("${SCRATCH_DIR}/archive-separator" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "';' nearer than the release, checkout")
    set(separator_version "${got_version}")
    set(separator_describe "${got_describe}")
    unpack_archive("${SCRATCH_DIR}/archive-separator" "HEAD" "${SCRATCH_DIR}/archive-separator-archive")
    expect_detect("${SCRATCH_DIR}/archive-separator-archive" "0.1.0" "^v0[.]1[.]0-1-g${HEX12}$" "';' nearer than the release, archive")
    expect_same("';' nearer than the release" "${separator_version}" "${separator_describe}")

    file(WRITE "${SCRATCH_DIR}/separator-file/cmake/ArchiveVersion.txt"
        "commit=0123456789abcdef0123456789abcdef01234567\ndescribe=v1.0.0-a;b\n")
    expect_detect("${SCRATCH_DIR}/separator-file" "0.0.0" "^0123456789ab$" "';' in a filled-in description")

    file(WRITE "${SCRATCH_DIR}/damaged/cmake/ArchiveVersion.txt"
        "commit=0123456789abZZZ\ndescribe=v1.0.0\n")
    expect_detect("${SCRATCH_DIR}/damaged" "0.0.0" "^unknown$" "damaged commit value")

    # A limit: a malformed tag git prefers on the release commit leaves the
    # archive without a version.
    make_archivable_repo("${SCRATCH_DIR}/colocated-malformed")
    create_tag("${SCRATCH_DIR}/colocated-malformed" "v1.2.3")
    create_tag("${SCRATCH_DIR}/colocated-malformed" "v1.2.3.1" -a -m "v1.2.3.1")
    expect_detect("${SCRATCH_DIR}/colocated-malformed" "1.2.3" "^v1[.]2[.]3$" "malformed tag on the release commit, checkout")
    unpack_archive("${SCRATCH_DIR}/colocated-malformed" "v1.2.3" "${SCRATCH_DIR}/colocated-malformed-archive")
    expect_detect("${SCRATCH_DIR}/colocated-malformed-archive" "0.0.0" "^${HEX12}$" "malformed tag on the release commit, archive")

    file(REMOVE_RECURSE "${SCRATCH_DIR}")
endif()

# --- the gate ----------------------------------------------------------------

if(checks LESS MINIMUM_CHECKS)
    message(SEND_ERROR "ProjectVersion: ran ${checks} checks, expected at least ${MINIMUM_CHECKS}")
elseif(failures GREATER 0)
    message(STATUS "ProjectVersion: ${failures} of ${checks} checks failed")
else()
    message(STATUS "ProjectVersion: ${checks} checks passed")
endif()
