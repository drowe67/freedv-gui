# Builds a dependency that was added with add_subdirectory()/FetchContent
# (wxWidgets, LibreSSL) as a release build would be, whatever FreeDV itself
# is being built as: optimised, and without debugging information.
#
# These are by far the largest part of a build that includes them, and
# nobody steps through them in a debugger. Their debugging information made
# up most of what CI caches of each such build, and of what's linked.
#
# This appends to the compiler's options rather than changing
# CMAKE_BUILD_TYPE or defining NDEBUG for them: wxWidgets leaves its
# assertion support out of the library when NDEBUG is defined, and a debug
# build of FreeDV then can't be linked against it.
#
# Usage: freedv_release_build_of_dependency(<its source directory>)
function(freedv_release_build_of_dependency directory)
    if(NOT (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"))
        return()
    endif()

    # The last -g and -O options on the command line are the ones that count.
    set(options -g0)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)
    if(build_type STREQUAL "DEBUG" OR build_type STREQUAL "")
        list(APPEND options -O2)
    endif()

    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC|SHARED|MODULE|OBJECT)_LIBRARY$" OR type STREQUAL "EXECUTABLE")
            target_compile_options(${target} PRIVATE ${options})
        endif()
    endforeach()

    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
        freedv_release_build_of_dependency("${subdirectory}")
    endforeach()
endfunction()
