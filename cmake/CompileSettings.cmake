# Compile settings shared by every target in the project.
#
#   include(CompileSettings)
#   scrap_apply_compile_settings(<target> [WARNINGS_AS_ERRORS])
#
# The options are private to the target, so linking against it does not
# change how the consumer compiles.

function(scrap_apply_compile_settings target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "WARNINGS_AS_ERRORS" "" "")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "scrap_apply_compile_settings: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    target_compile_features(${target} PUBLIC cxx_std_23)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<BOOL:${ARG_WARNINGS_AS_ERRORS}>:-Werror>
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3>
    )
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()
