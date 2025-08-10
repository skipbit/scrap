include(ExternalProject)

# CLI11 is a header-only library, so we can download and use it directly
ExternalProject_Add(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
    PREFIX ${CMAKE_BINARY_DIR}/dependencies/cli11
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
        -DCLI11_BUILD_TESTS=OFF
        -DCLI11_BUILD_EXAMPLES=OFF
        -DCLI11_BUILD_DOCS=OFF
)

set(CLI11_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)

# CLI11 is header-only, so we create an interface library
add_library(cli11_library INTERFACE IMPORTED)
set_target_properties(cli11_library PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CLI11_INCLUDE_DIR}"
)

add_dependencies(cli11_library cli11)