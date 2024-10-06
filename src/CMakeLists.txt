
add_executable(${PROJECT_NAME} ${SCRAP_SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/operation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/path.cpp
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

find_library(DROSS_LIBRARY NAMES dross PATHS ${PROJECT_BINARY_DIR}/lib NO_DEFAULT_PATH)
find_library(GIT2_LIBRARY NAMES git2 PATHS ${PROJECT_BINARY_DIR}/lib NO_DEFAULT_PATH)

target_include_directories(${PROJECT_NAME}
    SYSTEM BEFORE PRIVATE ${PROJECT_BINARY_DIR}/include
)

target_link_directories(${PROJECT_NAME}
    PRIVATE ${PROJECT_BINARY_DIR}/lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DROSS_LIBRARY}
    ${GIT2_LIBRARY}
)
