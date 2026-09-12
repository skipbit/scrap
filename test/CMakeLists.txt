# --- Google Test integration ---
find_package(GoogleTest REQUIRED)

add_executable(scrap_gtest
    unit/command/CommonTypesTest.cpp
    unit/command/CommandCatalogTest.cpp
    unit/command/DefaultHelpRendererTest.cpp
    unit/command/DefaultVersionRendererTest.cpp
    unit/command/BuiltinCommandResolverTest.cpp
    unit/command/ExternalCommandResolverTest.cpp
    unit/command/MetadataProtocolProviderTest.cpp
    unit/command/ProjectCommandResolverTest.cpp
    unit/command/RuntimeEnvironmentFactoryTest.cpp
    unit/command/ApplicationTest.cpp
    ${CMAKE_SOURCE_DIR}/src/command/Application.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandCatalog.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandResolver.cpp
    ${CMAKE_SOURCE_DIR}/src/command/ParserAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/command/HelpRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/command/VersionRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/command/DefaultHelpRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/command/DefaultVersionRenderer.cpp
    ${CMAKE_BINARY_DIR}/src/shared/constants/version.cpp
    ${CMAKE_SOURCE_DIR}/src/command/BuiltinCommandResolver.cpp
    ${CMAKE_SOURCE_DIR}/src/command/ExternalCommandResolver.cpp
    ${CMAKE_SOURCE_DIR}/src/command/ExternalMetadataProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/command/MetadataProtocolProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/command/ProjectCommandResolver.cpp
    ${CMAKE_SOURCE_DIR}/src/command/ScriptsReader.cpp
    ${CMAKE_SOURCE_DIR}/src/command/StubScriptsReader.cpp
    ${CMAKE_SOURCE_DIR}/src/command/RuntimeEnvironmentFactory.cpp
)

target_include_directories(scrap_gtest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(scrap_gtest PRIVATE GTest::gtest_main)
target_compile_features(scrap_gtest PUBLIC cxx_std_23)

target_compile_options(scrap_gtest PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

set_target_properties(scrap_gtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    CXX_EXTENSIONS OFF
)

include(GoogleTest)
gtest_discover_tests(scrap_gtest)

# --- CLI11ParserAdapter tests (requires CLI11 linkage) ---
find_package(CLI11 REQUIRED)

add_executable(scrap_gtest_cli11
    unit/command/CLI11ParserAdapterTest.cpp
    ${CMAKE_SOURCE_DIR}/src/command/driver/CLI11ParserAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/command/ParserAdapter.cpp
)

target_include_directories(scrap_gtest_cli11 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scrap_gtest_cli11 PRIVATE
    GTest::gtest_main
    CLI11::CLI11
)

target_compile_features(scrap_gtest_cli11 PUBLIC cxx_std_23)

target_compile_options(scrap_gtest_cli11 PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

set_target_properties(scrap_gtest_cli11 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    CXX_EXTENSIONS OFF
)

gtest_discover_tests(scrap_gtest_cli11)

# --- Project configuration tests (requires toml++ linkage) ---
find_package(TomlPlusPlus REQUIRED)

add_executable(scrap_gtest_project
    unit/project/ManifestParserTest.cpp
    unit/project/ProjectLocatorTest.cpp
    unit/project/TargetResolverTest.cpp
    unit/project/TempDirectory.cpp
    ${CMAKE_SOURCE_DIR}/src/project/ManifestError.cpp
    ${CMAKE_SOURCE_DIR}/src/project/ManifestParser.cpp
    ${CMAKE_SOURCE_DIR}/src/project/ProjectLocator.cpp
    ${CMAKE_SOURCE_DIR}/src/project/TargetResolver.cpp
)

target_include_directories(scrap_gtest_project PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(scrap_gtest_project PRIVATE
    GTest::gtest_main
    tomlplusplus::tomlplusplus
)

target_compile_features(scrap_gtest_project PUBLIC cxx_std_23)

target_compile_options(scrap_gtest_project PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

set_target_properties(scrap_gtest_project PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    CXX_EXTENSIONS OFF
)

gtest_discover_tests(scrap_gtest_project)

# --- CLI end-to-end tests (spawns the built scrap binary as a subprocess) ---
add_executable(scrap_e2e
    e2e/CliE2ETest.cpp
)

target_compile_definitions(scrap_e2e PRIVATE SCRAP_BINARY_PATH="$<TARGET_FILE:scrap>")

target_link_libraries(scrap_e2e PRIVATE GTest::gtest_main)
target_compile_features(scrap_e2e PUBLIC cxx_std_23)

target_compile_options(scrap_e2e PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

set_target_properties(scrap_e2e PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    CXX_EXTENSIONS OFF
)

# The e2e tests spawn the scrap binary itself, so it must be built first.
add_dependencies(scrap_e2e scrap)

gtest_discover_tests(scrap_e2e PROPERTIES LABELS e2e)

# --- CMake module tests (run in script mode) ---
#
# The version is resolved at configure time, so the logic behind it cannot be
# reached from a C++ test; the e2e check only confirms that whatever the
# configure produced is what the binary prints.
add_test(NAME ProjectVersionTest
    COMMAND ${CMAKE_COMMAND}
        -D "SCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/version-test"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersionTest.cmake
)
