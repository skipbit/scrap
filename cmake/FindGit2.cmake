include(ExternalProject)

ExternalProject_Add(libgit2
    GIT_REPOSITORY https://github.com/libgit2/libgit2.git
    GIT_TAG v1.8.1
    PREFIX ${CMAKE_BINARY_DIR}/dependencies/libgit2
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    BUILD_ALWAYS FALSE
)

set(GIT2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)
set(GIT2_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib)

add_library(git2_library SHARED IMPORTED)
set_target_properties(git2_library PROPERTIES
    IMPORTED_LOCATION ${GIT2_LIBRARY_DIR}/libgit2.dylib
)

add_dependencies(git2_library libgit2)
