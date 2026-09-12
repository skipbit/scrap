
# The version source is refreshed on every build rather than only at configure
# time: a commit, a tag or an edit moves the description, and the banner the
# binary prints should follow. Generating it once here as well gives the first
# build a file to compile.
set(SCRAP_VERSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/shared/constants/version.cpp")
set(SCRAP_VERSION_GENERATOR_ARGS
    -D "SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    -D "NAME=${PROJECT_NAME}"
    -D "INPUT=${CMAKE_CURRENT_SOURCE_DIR}/shared/constants/version.cpp.in"
    -D "OUTPUT=${SCRAP_VERSION_SOURCE}"
    -P "${CMAKE_SOURCE_DIR}/cmake/GenerateVersionSource.cmake"
)

execute_process(COMMAND ${CMAKE_COMMAND} ${SCRAP_VERSION_GENERATOR_ARGS})

add_custom_target(scrap_version_source ALL
    COMMAND ${CMAKE_COMMAND} ${SCRAP_VERSION_GENERATOR_ARGS}
    BYPRODUCTS "${SCRAP_VERSION_SOURCE}"
    COMMENT "Resolving the version"
    VERBATIM
)

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_sources(${PROJECT_NAME} PRIVATE
    # Generated version source
    ${SCRAP_VERSION_SOURCE}
    # Command architecture
    ${CMAKE_CURRENT_SOURCE_DIR}/command/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/BuiltinCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandCatalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/DefaultHelpRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/DefaultVersionRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ExternalCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ExternalMetadataProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/HelpRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/MetadataProtocolProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ParserAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/StubScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/VersionRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/driver/CLI11ParserAdapter.cpp
    # Project configuration (scrap.toml)
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ManifestError.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ManifestParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectLocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/TargetResolver.cpp
    # Composition root helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/command/RuntimeEnvironmentFactory.cpp
)

target_compile_options(${PROJECT_NAME} PUBLIC
    -Wall -Wextra -Wpedantic -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "scrap"
)

add_dependencies(${PROJECT_NAME} scrap_version_source)

find_package(CLI11 REQUIRED)
find_package(TomlPlusPlus REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CLI11::CLI11
    tomlplusplus::tomlplusplus
)
