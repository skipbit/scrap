include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.9.1
)

FetchContent_MakeAvailable(Catch2)

# Target: Catch2::Catch2WithMain
