include(ExternalProject)

ExternalProject_Add(dross
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/dross
    #GIT_REPOSITORY https://github.com/skipbit/dross.git
    #GIT_TAG main
    PREFIX ${CMAKE_SOURCE_DIR}/lib/dross/build/${CMAKE_BUILD_TYPE}
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    BUILD_ALWAYS TRUE
)

ExternalProject_Add(libgit2
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/git2
    #GIT_REPOSITORY https://github.com/libgit2/libgit2.git
    #GIT_TAG v1.8.1
    PREFIX ${CMAKE_SOURCE_DIR}/lib/git2/build/${CMAKE_BUILD_TYPE}
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
