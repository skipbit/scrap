# --- Google Test integration ---
find_package(GoogleTest REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# Helpers shared by the tests of more than one module
add_library(scrap_test_support STATIC
    support/TempDirectory.cpp
)
target_include_directories(scrap_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_test_support PRIVATE GTest::gtest)
scrap_apply_compile_settings(scrap_test_support)

add_executable(scrap_gtest
    unit/command/CommonTypesTest.cpp
    unit/command/CommandCatalogTest.cpp
    unit/command/DefaultHelpRendererTest.cpp
    unit/command/DefaultVersionRendererTest.cpp
    unit/command/BuiltinCommandResolverTest.cpp
    unit/command/ExternalCommandResolverTest.cpp
    unit/command/MetadataProtocolProviderTest.cpp
    unit/command/ProjectCommandResolverTest.cpp
    unit/command/ProjectDiagnosticTest.cpp
    unit/command/RuntimeEnvironmentFactoryTest.cpp
    unit/command/ApplicationTest.cpp
    unit/process/SubprocessTest.cpp
    unit/toolchain/CompilerIdentityTest.cpp
    unit/toolchain/SystemCompilerTest.cpp
)

target_link_libraries(scrap_gtest PRIVATE
    scrap_command
    scrap_process
    scrap_toolchain
    scrap_test_support
    GTest::gtest_main
)
scrap_apply_compile_settings(scrap_gtest)

include(GoogleTest)
gtest_discover_tests(scrap_gtest)

# --- CLI11ParserAdapter tests ---
add_executable(scrap_gtest_cli11
    unit/command/CLI11ParserAdapterTest.cpp
)

target_link_libraries(scrap_gtest_cli11 PRIVATE
    scrap_command
    GTest::gtest_main
)
scrap_apply_compile_settings(scrap_gtest_cli11)

gtest_discover_tests(scrap_gtest_cli11)

# --- Project configuration tests ---
add_executable(scrap_gtest_project
    unit/project/DiskProjectFileSystemTest.cpp
    unit/project/ManifestParserTest.cpp
    unit/project/ManifestSearch.cpp
    unit/project/ProjectCreatorTest.cpp
    unit/project/ProjectLoaderTest.cpp
    unit/project/ProjectLocatorTest.cpp
    unit/project/SourceCollectorTest.cpp
    unit/project/TargetResolverTest.cpp
)

target_link_libraries(scrap_gtest_project PRIVATE
    scrap_project
    scrap_test_support
    GTest::gtest_main
)
scrap_apply_compile_settings(scrap_gtest_project)

gtest_discover_tests(scrap_gtest_project)

# --- Compile command tests ---
add_executable(scrap_gtest_compile
    unit/compile/CompilationDatabaseTest.cpp
    unit/compile/CompilePlannerTest.cpp
    unit/compile/CompilerDriverTest.cpp
)

target_link_libraries(scrap_gtest_compile PRIVATE
    scrap_compile
    scrap_test_support
    GTest::gtest_main
)
scrap_apply_compile_settings(scrap_gtest_compile)

gtest_discover_tests(scrap_gtest_compile)

# --- Build step tests ---
add_executable(scrap_gtest_build
    unit/build/ProgramStepRunnerTest.cpp
    unit/build/SerialBuildTest.cpp
)

target_link_libraries(scrap_gtest_build PRIVATE
    scrap_build
    scrap_test_support
    GTest::gtest_main
)
scrap_apply_compile_settings(scrap_gtest_build)

gtest_discover_tests(scrap_gtest_build)

# --- CLI end-to-end tests (spawns the built scrap binary as a subprocess) ---
add_executable(scrap_e2e
    e2e/CliE2ETest.cpp
)

target_compile_definitions(scrap_e2e PRIVATE SCRAP_BINARY_PATH="$<TARGET_FILE:scrap>")

target_link_libraries(scrap_e2e PRIVATE GTest::gtest_main)
scrap_apply_compile_settings(scrap_e2e)

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
