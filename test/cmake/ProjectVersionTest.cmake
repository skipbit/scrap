# Tests for cmake/ProjectVersion.cmake.
#
# The version is resolved at configure time, before any C++ exists, so the
# logic is exercised by running the module in script mode (cmake -P). ctest
# registers this file; see test/CMakeLists.txt.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ProjectVersion.cmake")

set(checks 0)

macro(expect_version TAG EXPECTED)
    project_version_from_tag(actual "${TAG}")
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        message(SEND_ERROR "tag '${TAG}': expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

macro(expect_banner NAME VERSION DESCRIBE EXPECTED)
    project_version_banner(actual "${NAME}" "${VERSION}" "${DESCRIBE}")
    math(EXPR checks "${checks} + 1")
    if(NOT actual STREQUAL "${EXPECTED}")
        message(SEND_ERROR "banner('${VERSION}', '${DESCRIBE}'): expected '${EXPECTED}', got '${actual}'")
    endif()
endmacro()

# A release tag carries the version.
expect_version("v0.1.0" "0.1.0")
expect_version("v1.2.3" "1.2.3")
expect_version("v10.20.30" "10.20.30")

# A pre-release resolves to the release it leads to; the suffix stays in the
# description, which is where the banner shows it.
expect_version("v0.2.0-rc1" "0.2.0")

# Anything that is not a release tag leaves the tree unreleased.
expect_version("0.1.0" "0.0.0")
expect_version("v0.1" "0.0.0")
expect_version("nightly" "0.0.0")
expect_version("" "0.0.0")

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

message(STATUS "ProjectVersion: ${checks} checks passed")
