include_guard(GLOBAL)

# Shared helper for ExternalProject_Add-based dependencies that build via
# their own autotools ./configure && make invocation. That's a separate
# build system CMake just shells out to, so it never sees
# CMAKE_C_COMPILER_LAUNCHER/CMAKE_CXX_COMPILER_LAUNCHER (e.g. ccache) --
# those only apply to CMake's own compile rules. Baking the launcher into
# CC/CXX here gets these dependencies' builds cached the same way as the
# rest of the project on every platform, without relying on ccache's
# PATH-symlink workaround (create-symlink) being enabled on every CI
# runner that builds them.
#
# CMake-based ExternalProject_Add dependencies (e.g. codec2, sndfile) don't
# need this: they get a launcher by passing
# -DCMAKE_C_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER} (and the CXX
# equivalent) via their own CMAKE_ARGS instead, since their nested
# ./configure step is itself a CMake invocation.
if(CMAKE_C_COMPILER_LAUNCHER)
    set(EXTERNAL_PROJECT_CC "${CMAKE_C_COMPILER_LAUNCHER}\ ${CMAKE_C_COMPILER}")
else()
    set(EXTERNAL_PROJECT_CC "${CMAKE_C_COMPILER}")
endif()

if(CMAKE_CXX_COMPILER_LAUNCHER)
    set(EXTERNAL_PROJECT_CXX "${CMAKE_CXX_COMPILER_LAUNCHER}\ ${CMAKE_CXX_COMPILER}")
else()
    set(EXTERNAL_PROJECT_CXX "${CMAKE_CXX_COMPILER}")
endif()
