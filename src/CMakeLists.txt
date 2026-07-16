
# Generate version source from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/shared/constants/version.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/shared/constants/version.cpp"
    @ONLY
)

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_sources(${PROJECT_NAME} PRIVATE
    # Generated version source
    ${CMAKE_CURRENT_BINARY_DIR}/shared/constants/version.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ParserAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/StubScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/VersionRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/driver/CLI11ParserAdapter.cpp
    # Composition root helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/command/NullMetadataProvider.cpp
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

find_package(CLI11 REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CLI11::CLI11
)
