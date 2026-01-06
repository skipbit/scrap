include(FetchContent)

FetchContent_Declare(libgit2
    GIT_REPOSITORY https://github.com/libgit2/libgit2.git
    GIT_TAG v1.9.1
)

FetchContent_MakeAvailable(libgit2)

# libgit2's exported target is libgit2package, but it lacks BUILD_INTERFACE include dir
# Add the missing include directory for build-time usage
target_include_directories(libgit2package INTERFACE
    $<BUILD_INTERFACE:${libgit2_SOURCE_DIR}/include>
)

# Target: libgit2package
