# --- Google Test integration ---
find_package(GoogleTest REQUIRED)
include(GoogleTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)

# Helpers shared by the tests of more than one module
add_library(scrap_test_support STATIC
    support/TempDirectory.cpp
)
target_include_directories(scrap_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_test_support PRIVATE GTest::gtest)
scrap_apply_compile_settings(scrap_test_support WARNINGS_AS_ERRORS)

# A module's unit tests run in an executable that links that module alone,
# so a dependency the module does not declare fails to link there.
#
#   scrap_add_unit_test(<module> SOURCES <source>... [PROPERTIES <property> <value>...])
#
# The PROPERTIES are set on each test found in the executable.
function(scrap_add_unit_test module)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;PROPERTIES")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "scrap_add_unit_test: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    set(target scrap_gtest_${module})
    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE
        scrap_${module}
        scrap_test_support
        GTest::gtest_main
    )
    scrap_apply_compile_settings(${target} WARNINGS_AS_ERRORS)
    if(ARG_PROPERTIES)
        gtest_discover_tests(${target} PROPERTIES ${ARG_PROPERTIES})
    else()
        gtest_discover_tests(${target})
    endif()
endfunction()

scrap_add_unit_test(command SOURCES
    unit/command/CommonTypesTest.cpp
    unit/command/CommandCatalogTest.cpp
    unit/command/CLI11ParserAdapterTest.cpp
    unit/command/DefaultHelpRendererTest.cpp
    unit/command/DefaultVersionRendererTest.cpp
    unit/command/BuildProgressTest.cpp
    unit/command/BuiltinCommandResolverTest.cpp
    unit/command/ExternalCommandResolverTest.cpp
    unit/command/MetadataProtocolProviderTest.cpp
    unit/command/PrintableTextTest.cpp
    unit/command/ProjectCommandResolverTest.cpp
    unit/command/ProjectDiagnosticTest.cpp
    unit/command/RuntimeEnvironmentFactoryTest.cpp
    unit/command/ApplicationTest.cpp
)

scrap_add_unit_test(process SOURCES
    unit/process/SubprocessTest.cpp
)

scrap_add_unit_test(toolchain SOURCES
    unit/toolchain/CompilerIdentityTest.cpp
    unit/toolchain/SystemArchiverTest.cpp
    unit/toolchain/SystemCompilerTest.cpp
)

scrap_add_unit_test(file SOURCES
    unit/file/WholeFileTest.cpp
)

scrap_add_unit_test(project SOURCES
    unit/project/DiskProjectFileSystemTest.cpp
    unit/project/ManifestParserTest.cpp
    unit/project/ManifestSearch.cpp
    unit/project/ProjectCreatorTest.cpp
    unit/project/ProjectLoaderTest.cpp
    unit/project/ProjectLocatorTest.cpp
    unit/project/SourceCollectorTest.cpp
    unit/project/TargetResolverTest.cpp
)

scrap_add_unit_test(compile SOURCES
    unit/compile/CompilationDatabaseTest.cpp
    unit/compile/CompilePlannerTest.cpp
    unit/compile/CompilerDriverTest.cpp
    unit/compile/DependencyFileTest.cpp
    unit/compile/HeaderSearchTest.cpp
    unit/compile/IncludeDirectivesTest.cpp
)

# A test that runs steps at once waits on events alone, so a build that runs
# them one at a time would leave it waiting; the limit ends such a wait.
scrap_add_unit_test(build SOURCES
    unit/build/BuildOutputTest.cpp
    unit/build/BuildStepsTest.cpp
    unit/build/ParallelismTest.cpp
    unit/build/ProgramStepRunnerTest.cpp
    unit/build/StepHistoryTest.cpp
    unit/build/StepObservationTest.cpp
    unit/build/StepRecordTest.cpp
    PROPERTIES TIMEOUT 60
)

# --- CLI end-to-end tests (spawns the built scrap binary as a subprocess) ---
add_executable(scrap_e2e
    e2e/CliE2ETest.cpp
)

# The e2e tests build a project with the compiler this tree was built with:
# the fixed PATH they give the child holds no compiler of its own.
target_compile_definitions(scrap_e2e PRIVATE
    SCRAP_BINARY_PATH="$<TARGET_FILE:scrap>"
    SCRAP_TEST_CXX="${CMAKE_CXX_COMPILER}"
)

target_link_libraries(scrap_e2e PRIVATE
    scrap_test_support
    GTest::gtest_main
)
scrap_apply_compile_settings(scrap_e2e WARNINGS_AS_ERRORS)

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
