include(FetchContent)

FetchContent_Declare(libgit2
    GIT_REPOSITORY https://github.com/libgit2/libgit2.git
    GIT_TAG v1.9.2
)

# Disable nanosecond timestamp support — feature detection fails on
# Ubuntu 24.04 with recent glibc (GIT_USE_NSEC / struct stat mismatch)
set(USE_NSEC OFF CACHE BOOL "" FORCE)

# Disable libgit2's own tests (they also reference st_*_nsec fields).
# Save parent BUILD_TESTS value and restore after FetchContent.
set(_parent_BUILD_TESTS ${BUILD_TESTS})
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libgit2)
set(BUILD_TESTS ${_parent_BUILD_TESTS} CACHE BOOL "" FORCE)

# libgit2's exported target is libgit2package, but it lacks BUILD_INTERFACE include dir
# Add the missing include directory for build-time usage
target_include_directories(libgit2package INTERFACE
    $<BUILD_INTERFACE:${libgit2_SOURCE_DIR}/include>
)

# Target: libgit2package
