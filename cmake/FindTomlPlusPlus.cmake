include(FetchContent)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(tomlplusplus)

# The project prohibits exceptions (docs/CODINGSTYLE.md), so toml++ must report
# failures through its parse_result rather than throwing toml::parse_error.
# The setting selects which inline namespace the headers define (toml::v3 with
# it, toml::v3::ex without), so it is set on the library target itself: every
# consumer then agrees, and no target can link against a differently built view
# of the same headers.
target_compile_definitions(tomlplusplus_tomlplusplus INTERFACE TOML_EXCEPTIONS=0)

# Target: tomlplusplus::tomlplusplus
