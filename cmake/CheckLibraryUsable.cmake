# CheckLibraryUsable(<result_var>
#                    SYMBOL <symbol> HEADER <header>
#                    LIBRARIES <lib>... [INCLUDE_DIRS <dir>...])
#
# Verifies that a library located via find_library()/find_package() can
# actually be compiled against and linked into a program using the current
# toolchain. find_library() will happily return libraries built for the
# wrong OS or architecture (e.g. host libraries when cross-compiling, or
# single-architecture libraries when building macOS universal binaries);
# callers should fall back to building the dependency themselves when
# <result_var> is false.
include(CheckSymbolExists)
include(CMakePushCheckState)

function(CheckLibraryUsable RESULT_VAR)
    cmake_parse_arguments(ARG "" "SYMBOL;HEADER" "LIBRARIES;INCLUDE_DIRS" ${ARGN})

    foreach(_lib IN LISTS ARG_LIBRARIES)
        if(NOT _lib OR (NOT TARGET ${_lib} AND NOT EXISTS "${_lib}"))
            set(${RESULT_VAR} 0 PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Key the cached result on everything that affects it so that the check
    # reruns if the user points CMake at a different copy of the library.
    string(MD5 _key "${ARG_SYMBOL};${ARG_HEADER};${ARG_LIBRARIES};${ARG_INCLUDE_DIRS};${CMAKE_OSX_ARCHITECTURES}")
    set(_cache_var "FREEDV_LIBRARY_USABLE_${RESULT_VAR}_${_key}")

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_QUIET ON)
    set(CMAKE_REQUIRED_LIBRARIES ${ARG_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES ${ARG_INCLUDE_DIRS})
    check_symbol_exists(${ARG_SYMBOL} "${ARG_HEADER}" ${_cache_var})
    cmake_pop_check_state()

    if(${_cache_var})
        set(${RESULT_VAR} 1 PARENT_SCOPE)
    else()
        message(STATUS "  ${ARG_LIBRARIES} cannot be linked with the current toolchain, ignoring")
        set(${RESULT_VAR} 0 PARENT_SCOPE)
    endif()
endfunction()
