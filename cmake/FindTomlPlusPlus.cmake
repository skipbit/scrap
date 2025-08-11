include(ExternalProject)

ExternalProject_Add(tomlplusplus_external
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    PREFIX ${CMAKE_BINARY_DIR}/dependencies/tomlplusplus
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
)

ExternalProject_Get_Property(tomlplusplus_external source_dir)
set(TOMLPLUSPLUS_INCLUDE_DIR ${source_dir}/include)

add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE ${TOMLPLUSPLUS_INCLUDE_DIR})
target_compile_features(tomlplusplus INTERFACE cxx_std_17)

add_dependencies(tomlplusplus tomlplusplus_external)
