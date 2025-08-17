# Find Catch2 package
find_package(Catch2 REQUIRED)

# Collect test source files
set(TEST_SOURCES
    # Helpers
    helpers/TestPresenter.cpp
    helpers/FileSystemHelper.cpp
    
    # Unit tests - shared components (basic tests that work)
    unit/shared/command/OperationTest.cpp
    
    # Template and Configuration tests are created but need implementations
    # to be linked properly. They will be enabled once the template and
    # configuration modules have their .cpp files implemented.
    # 
    # These tests are compatible with the actual APIs now but missing symbols:
    # unit/template/model/TemplateTest.cpp
    # unit/template/service/TemplateServiceTest.cpp  
    # unit/template/service/TemplateProcessorTest.cpp
    # unit/configuration/model/ConfigurationTest.cpp
    # unit/configuration/service/ConfigurationServiceTest.cpp
)

# Create test executable
add_executable(scrap_test ${TEST_SOURCES})

# Include directories
target_include_directories(scrap_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src  # For generated version.cpp
)

# Link libraries (minimal dependencies for now)
target_link_libraries(scrap_test PRIVATE
    catch2_library
)

# Add dependencies
add_dependencies(scrap_test 
    catch2_external
)

# C++ standard
target_compile_features(scrap_test PUBLIC cxx_std_23)

# Compile options
target_compile_options(scrap_test PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

# Set runtime output directory
set_target_properties(scrap_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    CXX_EXTENSIONS OFF
)

# Register with CTest
add_test(
    NAME scrap_unit_tests 
    COMMAND scrap_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Create custom 'run_tests' target for convenient testing
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
    DEPENDS scrap_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running SCRAP unit tests..."
)

# Set test properties
set_tests_properties(scrap_unit_tests PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "SCRAP_TEST_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)