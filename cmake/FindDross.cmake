include(ExternalProject)

ExternalProject_Add(dross
    GIT_REPOSITORY https://github.com/skipbit/dross.git
    GIT_TAG main
    PREFIX ${CMAKE_BINARY_DIR}/dependencies/dross
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
)

set(DROSS_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)
set(DROSS_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib)

add_library(dross_library SHARED IMPORTED)
set_target_properties(dross_library PROPERTIES
    IMPORTED_LOCATION ${DROSS_LIBRARY_DIR}/libdross${CMAKE_SHARED_LIBRARY_SUFFIX}
)

add_dependencies(dross_library dross)
