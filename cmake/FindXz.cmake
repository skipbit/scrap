# Provides scrap_liblzma, the liblzma the code reading xz files links against.
#
# It is built from source and linked in, on macOS too, whose SDK carries no
# liblzma.

enable_language(C)
include(FetchContent)

# The options stay in this function's scope, away from the rest of the build.
function(scrap_fetch_liblzma)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    # CMake before 3.24 takes this option as part of the hash, and keeps the
    # timestamps of an archive without being asked.
    set(keep_archive_timestamps "")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        set(keep_archive_timestamps DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    FetchContent_Declare(xz
        URL https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.gz
        URL_HASH SHA256=0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9
        ${keep_archive_timestamps}
    )

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    # Decoding .xz is all that is used. This one is a cache entry because the
    # project declares it with set(CACHE), which before CMake 3.21 discards a
    # variable of the same name.
    set(XZ_ENCODERS "" CACHE STRING "")
    set(XZ_MICROLZMA_ENCODER OFF)
    set(XZ_MICROLZMA_DECODER OFF)
    set(XZ_LZIP_DECODER OFF)
    set(XZ_NLS OFF)
    set(XZ_DOC OFF)
    set(XZ_TOOL_XZ OFF)
    set(XZ_TOOL_XZDEC OFF)
    set(XZ_TOOL_LZMADEC OFF)
    set(XZ_TOOL_LZMAINFO OFF)
    set(XZ_TOOL_SCRIPTS OFF)
    FetchContent_MakeAvailable(xz)
endfunction()

scrap_fetch_liblzma()
FetchContent_GetProperties(xz)

add_library(scrap_liblzma INTERFACE)
target_link_libraries(scrap_liblzma INTERFACE liblzma)

# Its headers are system headers here, as they are for every other dependency.
target_include_directories(scrap_liblzma SYSTEM INTERFACE "${xz_SOURCE_DIR}/src/liblzma/api")
