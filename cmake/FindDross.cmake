include(FetchContent)

FetchContent_Declare(dross
    GIT_REPOSITORY https://github.com/skipbit/dross.git
    GIT_TAG main
)

FetchContent_MakeAvailable(dross)

# Treat dross headers as system includes to suppress third-party
# warnings when compiling with -Wpedantic -Werror
get_target_property(_dross_includes dross INTERFACE_INCLUDE_DIRECTORIES)
if(_dross_includes)
    set_property(TARGET dross PROPERTY INTERFACE_INCLUDE_DIRECTORIES "")
    target_include_directories(dross SYSTEM INTERFACE ${_dross_includes})
endif()

# Target: dross
