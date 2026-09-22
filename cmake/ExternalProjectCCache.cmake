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

# CMAKE_C_COMPILER/CMAKE_CXX_COMPILER above can resolve to the Xcode
# toolchain's raw absolute cc/c++ path (inside XcodeDefault.xctoolchain)
# rather than /usr/bin/cc or `xcrun cc`. autoconf's own compiler-works check
# invokes that path directly, without going through xcrun, so it doesn't
# auto-discover the default SDK and fails with "ld: library 'System' not
# found". Pass -isysroot explicitly via CFLAGS/CXXFLAGS so these
# dependencies' ./configure can actually link its test program.
#
# Separately, autoconf determines its own CPP command ("checking how to run
# the C preprocessor") as plain "$CC -E", without CFLAGS -- so header checks
# that go through CPP directly (e.g. AC_HEADER_STDC's AC_EGREP_HEADER calls)
# still can't find SDK headers even once CFLAGS above is fixed. Callers must
# also pass this via CPPFLAGS, or those checks silently report headers/
# functions as missing (e.g. STDC_HEADERS ends up undefined even though
# string.h etc. are all present) and dependents can hit obscure build
# failures as a result (e.g. old K&R-style fallback code paths that assume
# an ANSI-less libc and don't compile under a modern, strict C compiler).
if(APPLE AND CMAKE_OSX_SYSROOT)
    set(EXTERNAL_PROJECT_APPLE_ISYSROOT -isysroot\ ${CMAKE_OSX_SYSROOT})
else()
    set(EXTERNAL_PROJECT_APPLE_ISYSROOT "")
endif()
