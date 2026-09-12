cmake_minimum_required(VERSION 3.20)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Git tags are the source of truth for the version, so it is resolved here
# rather than written down. See cmake/ProjectVersion.cmake.
include(ProjectVersion)
scrap_version_detect(SCRAP_VERSION SCRAP_GIT_DESCRIBE "${CMAKE_CURRENT_SOURCE_DIR}")

project(scrap VERSION ${SCRAP_VERSION} LANGUAGES CXX)

scrap_version_banner(SCRAP_VERSION_BANNER
    "${PROJECT_NAME}" "${PROJECT_VERSION}" "${SCRAP_GIT_DESCRIBE}")
message(STATUS "Version: ${SCRAP_VERSION_BANNER}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_TESTS "Build unit tests" ON)

add_subdirectory(src)

# Add tests if enabled
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
