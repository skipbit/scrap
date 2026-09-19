
# The version source is refreshed on every build rather than only at configure
# time: a commit, a tag or an edit moves the description, and the banner the
# binary prints should follow. Generating it once here as well gives the first
# build a file to compile.
set(SCRAP_VERSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/shared/constants/version.cpp")
set(SCRAP_VERSION_GENERATOR_ARGS
    -D "SOURCE_DIR=${PROJECT_SOURCE_DIR}"
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

find_package(CLI11 REQUIRED)
find_package(TomlPlusPlus REQUIRED)

# Each module is built once as a library, and the binary and the tests link
# against it.

# Project configuration (scrap.toml)
add_library(scrap_project STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/project/DefaultTemplate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ManifestError.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ManifestParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectCreator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectFileSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectLocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/SourceCollector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/TargetResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/driver/DiskProjectFileSystem.cpp
)
target_include_directories(scrap_project PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_project PRIVATE tomlplusplus::tomlplusplus)
scrap_apply_compile_settings(scrap_project WARNINGS_AS_ERRORS)

# Toolchain detection
add_library(scrap_toolchain STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/SystemCompiler.cpp
)
target_include_directories(scrap_toolchain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
scrap_apply_compile_settings(scrap_toolchain WARNINGS_AS_ERRORS)

# Command architecture, with the version banner it prints
add_library(scrap_command STATIC
    ${SCRAP_VERSION_SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/command/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/BuildCommandHandler.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/command/NewCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ParserAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectDiagnostic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/RuntimeEnvironmentFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/StubScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/VersionRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/driver/CLI11ParserAdapter.cpp
)
target_include_directories(scrap_command PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The command headers name project types; the toolchain and CLI11 stay inside.
target_link_libraries(scrap_command
    PUBLIC scrap_project
    PRIVATE scrap_toolchain CLI11::CLI11
)
add_dependencies(scrap_command scrap_version_source)
scrap_apply_compile_settings(scrap_command WARNINGS_AS_ERRORS)

# Composition root
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE scrap_command)
scrap_apply_compile_settings(${PROJECT_NAME} WARNINGS_AS_ERRORS)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME "scrap"
)
