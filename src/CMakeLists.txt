
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
find_package(Threads REQUIRED)
find_package(LibCurl REQUIRED)
find_package(Xz REQUIRED)

# Each module is built once as a library, and the binary and the tests link
# against it.

# Reading and writing whole files
add_library(scrap_file STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/file/PosixFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file/StagedFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file/WholeFile.cpp
)
target_include_directories(scrap_file PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
scrap_apply_compile_settings(scrap_file WARNINGS_AS_ERRORS)

# Digests of bytes and files, depending on nothing else in scrap
add_library(scrap_digest STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/digest/DigestText.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digest/Sha256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/digest/Sha256File.cpp
)
target_include_directories(scrap_digest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
scrap_apply_compile_settings(scrap_digest WARNINGS_AS_ERRORS)

# Reading xz files, depending on nothing else in scrap
add_library(scrap_xz STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/xz/XzFile.cpp
)
target_include_directories(scrap_xz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_xz PRIVATE scrap_liblzma)
scrap_apply_compile_settings(scrap_xz WARNINGS_AS_ERRORS)

# Reading tar archives and extracting them, depending on nothing else in scrap
add_library(scrap_tar STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tar/Descriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tar/TarExtractor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tar/TarPath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tar/TarReader.cpp
)
target_include_directories(scrap_tar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
scrap_apply_compile_settings(scrap_tar WARNINGS_AS_ERRORS)

# Reading JSON, depending on nothing else in scrap
add_library(scrap_json STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/json/JsonParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json/JsonValue.cpp
)
target_include_directories(scrap_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
scrap_apply_compile_settings(scrap_json WARNINGS_AS_ERRORS)

# Fetching URLs into files
add_library(scrap_fetch STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/fetch/CertificateBundle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fetch/FetchErrorText.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fetch/UrlPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fetch/driver/CurlFetch.cpp
)
target_include_directories(scrap_fetch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_fetch PRIVATE scrap_file scrap_libcurl)
scrap_apply_compile_settings(scrap_fetch WARNINGS_AS_ERRORS)

# Fetching ingots from an OCI registry
add_library(scrap_registry STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/registry/BearerChallenge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/registry/IngotLocation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/registry/IngotManifest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/registry/RegistryFetch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/registry/TokenRequest.cpp
)
target_include_directories(scrap_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_registry PUBLIC scrap_fetch PRIVATE scrap_digest scrap_file scrap_json)
scrap_apply_compile_settings(scrap_registry WARNINGS_AS_ERRORS)

# Project configuration (scrap.toml)
add_library(scrap_project STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/project/DefaultTemplate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/project/LanguageStandard.cpp
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
target_link_libraries(scrap_project PRIVATE scrap_file scrap_toolchain tomlplusplus::tomlplusplus)
scrap_apply_compile_settings(scrap_project WARNINGS_AS_ERRORS)

# Running other programs
add_library(scrap_process STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/process/ExecutableFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/process/Subprocess.cpp
)
target_include_directories(scrap_process PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
scrap_apply_compile_settings(scrap_process WARNINGS_AS_ERRORS)

# Toolchains: detecting the system's, reading specifiers and the index,
# keeping the global default, and finding the installed ones
add_library(scrap_toolchain STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/CompilerIdentity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/GlobalConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/GlobalConfigFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/HostEnvironment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/HostGlibc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/IngotSelection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/InstallRecord.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/InstalledToolchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/MacSdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ProgramAnswer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ProgramSearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/RuntimeLibraryName.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ScrapHomeLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/SystemArchiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/SystemCompiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ToolchainChoice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ToolchainIndexParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ToolchainName.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ToolchainSpecifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/ToolchainVersion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/toolchain/driver/TomlText.cpp
)
target_include_directories(scrap_toolchain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_toolchain PRIVATE scrap_digest scrap_file scrap_process tomlplusplus::tomlplusplus)
scrap_apply_compile_settings(scrap_toolchain WARNINGS_AS_ERRORS)

# Installing toolchains: obtaining the index, and fetching, checking and placing
# what it offers
add_library(scrap_install STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/install/IndexCopy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/install/IngotInstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/install/IngotUnpack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/install/UnpackedToolchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/install/UtcTime.cpp
)
target_include_directories(scrap_install PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(scrap_install
    PUBLIC scrap_toolchain scrap_fetch
    PRIVATE scrap_digest scrap_file scrap_registry scrap_tar scrap_xz
)
scrap_apply_compile_settings(scrap_install WARNINGS_AS_ERRORS)

# Compile commands
add_library(scrap_compile STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/compile/CompilationDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile/CompilePlanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile/CompilerDriver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile/DependencyFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile/HeaderSearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compile/IncludeDirectives.cpp
)
target_include_directories(scrap_compile PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The compile headers name project and toolchain types.
target_link_libraries(scrap_compile PUBLIC scrap_project scrap_toolchain PRIVATE scrap_file)
scrap_apply_compile_settings(scrap_compile WARNINGS_AS_ERRORS)

# Running the steps of a build
add_library(scrap_build STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/build/BuildOutput.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/BuildReporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/BuildSteps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/Parallelism.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/StepHistory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/StepObservation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/StepRecord.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/build/StepRunner.cpp
)
target_include_directories(scrap_build PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The build headers name compile types; running a program and the threads
# that run steps together stay inside.
target_link_libraries(scrap_build PUBLIC scrap_compile PRIVATE scrap_file scrap_process Threads::Threads)
scrap_apply_compile_settings(scrap_build WARNINGS_AS_ERRORS)

# Command architecture, with the version banner it prints
add_library(scrap_command STATIC
    ${SCRAP_VERSION_SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/command/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/BuildCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/BuildProgress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/BuiltinCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CleanCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandCatalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandPath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/CompilerChoice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/DefaultHelpRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/DefaultVersionRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ExternalCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ExternalMetadataProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/HelpRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/HelpRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/MetadataProtocolProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/NewCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ParserAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/PrintableText.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectArgument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectBuild.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectCommandResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ProjectDiagnostic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/RunCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/RuntimeEnvironmentFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ScrapHome.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/StubScriptsReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ToolchainDiagnostic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ToolchainInstallCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ToolchainInstallDiagnostic.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ToolchainSelectCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/ToolchainUnselectCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/VersionRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/WriteFailureHint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command/driver/CLI11ParserAdapter.cpp
)
target_include_directories(scrap_command PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The command headers other libraries include name project, compile and
# build types; the toolchain, running a program and CLI11 stay inside, so
# only headers used within the command library may name them.
target_link_libraries(scrap_command
    PUBLIC scrap_project scrap_compile scrap_build
    PRIVATE scrap_toolchain scrap_process scrap_install scrap_fetch CLI11::CLI11
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
