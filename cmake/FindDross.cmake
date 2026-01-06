include(FetchContent)

FetchContent_Declare(dross
    GIT_REPOSITORY https://github.com/skipbit/dross.git
    GIT_TAG main
)

FetchContent_MakeAvailable(dross)

# Target: dross
