
add_executable(${PROJECT_NAME} ${SCRAP_SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    # Shared command components
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/Command.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/Operation.cpp
    # Repository components
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/model/Repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/driver/GitDriver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/driver/LibGitRepository.cpp
    # Toolchain components
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/command/ToolchainCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/command/ListCommand.cpp
)

target_compile_options(${PROJECT_NAME} PUBLIC
    -Wall -Wextra -Wpedantic -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "scrap"
)

find_package(dross REQUIRED)
find_package(git2 REQUIRED)
find_package(cli11 REQUIRED)

add_dependencies(scrap dross_library git2_library cli11_library)

target_include_directories(${PROJECT_NAME}
    SYSTEM BEFORE PRIVATE ${DROSS_INCLUDE_DIR} ${GIT2_INCLUDE_DIR}
)

target_link_directories(${PROJECT_NAME}
    PRIVATE ${DROSS_LIBRARY_DIR} ${GIT2_LIBRARY_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    dross_library
    git2_library
    cli11_library
)
