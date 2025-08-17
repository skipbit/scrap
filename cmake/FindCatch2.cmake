include(ExternalProject)

# Catch2 v3.9.1 - Modern C++ test framework
ExternalProject_Add(catch2_external
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.9.1
    PREFIX ${CMAKE_BINARY_DIR}/dependencies/catch2
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
        -DCATCH_BUILD_TESTING=OFF
        -DCATCH_BUILD_EXAMPLES=OFF
        -DCATCH_INSTALL_DOCS=OFF
        -DCATCH_INSTALL_EXTRAS=ON
)

set(CATCH2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)
set(CATCH2_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib)

# Create imported target for Catch2
add_library(catch2_library INTERFACE IMPORTED)

# Set library names based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CATCH2_LIB_NAMES "Catch2d;Catch2Maind")
else()
    set(CATCH2_LIB_NAMES "Catch2;Catch2Main")
endif()

set_target_properties(catch2_library PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CATCH2_INCLUDE_DIR}"
    INTERFACE_LINK_DIRECTORIES "${CATCH2_LIBRARY_DIR}"
    INTERFACE_LINK_LIBRARIES "${CATCH2_LIB_NAMES}"
)

add_dependencies(catch2_library catch2_external)
