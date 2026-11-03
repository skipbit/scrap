# Provides scrap_libcurl, the libcurl the fetching code links against.
#
# On macOS that is the one the operating system carries. Elsewhere libcurl and
# Mbed TLS are built from source and linked in, so that the binary needs no
# library at run time beyond the C library.

add_library(scrap_libcurl INTERFACE)

if(APPLE)
    target_link_libraries(scrap_libcurl INTERFACE curl)
    return()
endif()

enable_language(C)
include(FetchContent)

# The options stay in this function's scope, away from the rest of the build.
function(scrap_fetch_libcurl)
    # Their projects declare old policies, under which option() and a cache
    # entry set over a variable would discard what is set here.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)

    # CMake before 3.24 takes this option as part of the hash, and keeps the
    # timestamps of an archive without being asked.
    set(keep_archive_timestamps "")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        set(keep_archive_timestamps DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()

    # The archives are used rather than the repositories: they carry the files
    # the repositories leave to be generated, and Mbed TLS's submodules.
    FetchContent_Declare(mbedtls
        URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-4.1.1/mbedtls-4.1.1.tar.bz2
        URL_HASH SHA256=3359a349e23db3d5536fcee032ae7b2ecbfc08972fab643089b5cbf2a375c98c
        ${keep_archive_timestamps}
    )
    FetchContent_Declare(curl
        URL https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz
        URL_HASH SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
        ${keep_archive_timestamps}
    )

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    # A warning a newer compiler adds to their code does not stop this build.
    set(ENABLE_PROGRAMS OFF)
    set(ENABLE_TESTING OFF)
    set(MBEDTLS_FATAL_WARNINGS OFF)
    set(TF_PSA_CRYPTO_FATAL_WARNINGS OFF)
    FetchContent_MakeAvailable(mbedtls)

    # Naming Mbed TLS here keeps libcurl from finding another copy on the host,
    # whose headers would not match the libraries linked in.
    set(CURL_USE_MBEDTLS ON)
    set(CURL_USE_OPENSSL OFF)
    set(CURL_USE_PKGCONFIG OFF)
    set(MBEDTLS_INCLUDE_DIR "${mbedtls_SOURCE_DIR}/include")
    set(MBEDTLS_LIBRARY mbedtls)
    set(MBEDX509_LIBRARY mbedx509)
    set(MBEDCRYPTO_LIBRARY tfpsacrypto)

    # The path of the build machine's certificates would mean nothing on the
    # host the binary runs on; the fetching code finds them when it runs. These
    # two are cache entries because libcurl clears 'none' from the cache only,
    # and a variable left behind would be compiled in as a path named none.
    set(CURL_CA_BUNDLE none CACHE STRING "")
    set(CURL_CA_PATH none CACHE STRING "")

    # Only HTTP and HTTPS, and nothing the host may or may not provide.
    set(HTTP_ONLY ON)
    set(CURL_BROTLI OFF)
    set(CURL_DISABLE_LDAP ON)
    set(CURL_USE_LIBPSL OFF)
    set(CURL_USE_LIBSSH2 OFF)
    set(CURL_ZLIB OFF)
    set(CURL_ZSTD OFF)
    set(USE_LIBIDN2 OFF)
    set(USE_NGHTTP2 OFF)

    set(BUILD_CURL_EXE OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_LIBCURL_DOCS OFF)
    set(BUILD_MISC_DOCS OFF)
    set(BUILD_STATIC_LIBS ON)
    set(CURL_DISABLE_INSTALL ON)
    set(CURL_ENABLE_EXPORT_TARGET OFF)
    set(ENABLE_CURL_MANUAL OFF)
    set(PICKY_COMPILER OFF)
    FetchContent_MakeAvailable(curl)
endfunction()

scrap_fetch_libcurl()
FetchContent_GetProperties(curl)

# Its headers are system headers here too, as they are on macOS.
set_target_properties(libcurl_static PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${curl_SOURCE_DIR}/include"
)
target_link_libraries(scrap_libcurl INTERFACE CURL::libcurl)

# Target: scrap_libcurl
