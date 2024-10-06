include(ExternalProject)

ExternalProject_Add(dross
    GIT_REPOSITORY https://github.com/skipbit/dross.git
    GIT_TAG main
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    BUILD_ALWAYS TRUE
)

ExternalProject_Add(libgit2
    GIT_REPOSITORY https://github.com/libgit2/libgit2.git
    GIT_TAG v1.8.1
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    BUILD_ALWAYS TRUE
)

add_custom_target(update_submodules
    COMMAND ${CMAKE_COMMAND} -E echo "Checking submodules ..."
    COMMAND git submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Updating Git submodules"
    VERBATIM
)

add_custom_target(prepare ALL DEPENDS update_submodules)
