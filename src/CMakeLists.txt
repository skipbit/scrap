
add_executable(${PROJECT_NAME} ${SCRAP_SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/git_driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/libgit_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/toolchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/list.cpp
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

add_dependencies(scrap dross_library git2_library)

target_include_directories(${PROJECT_NAME}
    SYSTEM BEFORE PRIVATE ${DROSS_INCLUDE_DIR} ${GIT2_INCLUDE_DIR}
)

target_link_directories(${PROJECT_NAME}
    PRIVATE ${DROSS_LIBRARY_DIR} ${GIT2_LIBRARY_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    dross_library
    git2_library
)
