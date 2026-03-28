include(FetchContent)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Targets: GTest::gtest, GTest::gtest_main, GTest::gmock, GTest::gmock_main
