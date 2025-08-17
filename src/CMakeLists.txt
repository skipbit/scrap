
# Generate version source from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/shared/constants/version.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/shared/constants/version.cpp"
    @ONLY
)

add_executable(${PROJECT_NAME} ${SCRAP_SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    # Generated version source
    ${CMAKE_CURRENT_BINARY_DIR}/shared/constants/version.cpp
    # Shared command components
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/Operation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/CompositeOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/CommandDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/ApplicationCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/HelpCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/CommandOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/ParsedOptions.cpp
    # CLI driver implementations
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/driver/CLI11Parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/driver/CLI11CommandDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/command/driver/PresenterFormatter.cpp
    # Presentation components
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/presentation/driver/ConsolePresenter.cpp
    # Repository components
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/model/Repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/driver/GitDriver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/repository/driver/LibGitRepository.cpp
    # Toolchain components
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ToolchainModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/model/Toolchain.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/service/ToolchainService.cpp  # Old implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/service/MockToolchainService.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/driver/ToolchainRepository.cpp  # Old implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/command/ToolchainOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/command/ListOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/command/InstallOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/command/SelectOperation.cpp
    # Project components
    ${CMAKE_CURRENT_SOURCE_DIR}/project/ProjectModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/model/Project.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/service/MockProjectService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/command/NewOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/command/BuildOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/command/RunOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/command/CleanOperation.cpp
    # Configuration components
    ${CMAKE_CURRENT_SOURCE_DIR}/configuration/ConfigurationModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configuration/model/ConfigurationSource.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configuration/service/DefaultConfigurationService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/configuration/driver/TomlPlusPlusDriver.cpp
    # Template components
    ${CMAKE_CURRENT_SOURCE_DIR}/template/TemplateModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template/model/Template.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template/service/TemplateService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template/service/TemplateProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template/command/TemplateOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template/command/ListOperation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/template/command/UpdateOperation.cpp
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

find_package(dross REQUIRED)
find_package(git2 REQUIRED)
find_package(cli11 REQUIRED)
find_package(tomlplusplus REQUIRED)

add_dependencies(scrap dross_library git2_library cli11_library tomlplusplus_external)

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
    tomlplusplus
)
