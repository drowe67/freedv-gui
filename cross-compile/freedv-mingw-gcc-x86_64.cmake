set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(triple ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32)

set(CMAKE_C_COMPILER ${triple}-gcc)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER ${triple}-g++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(CMAKE_AR ${triple}-ar)
set(CMAKE_RANLIB ${triple}-ranlib)
set(CMAKE_RC_COMPILER ${triple}-windres)

# For make package use.
set(CMAKE_OBJDUMP ${triple}-objdump)

# Extra runtime libraries needed with Ubuntu's gcc-mingw-w64:
#  - libssp.a: Ubuntu's gcc bakes in _FORTIFY_SOURCE hardening by default
#    (even when cross-compiling), so calls like memcpy() with a
#    compile-time-known destination size get rewritten to __memcpy_chk()
#    etc. On Linux those come from glibc; mingw-w64 has no libc of its
#    own providing them, so without this they fail to link. libssp.a
#    (shipped by gcc-mingw-w64-x86-64) provides the fortify-source
#    runtime for mingw targets.
#  - libucrt.a/libucrtbase.a: needed for newer CRT additions like C11's
#    timespec_get(), which msvcrt.dll (mingw-w64's other, older default
#    CRT choice) doesn't have.
#
# These must be appended via CMAKE_<LANG>_STANDARD_LIBRARIES, not
# CMAKE_EXE_LINKER_FLAGS/CMAKE_SHARED_LINKER_FLAGS: CMake's Makefile
# generator places CMAKE_*_LINKER_FLAGS content *before* the target's
# own object files and libraries on the link line, but ld only pulls a
# static archive's symbols in to satisfy references it already knows
# are outstanding at that point in a left-to-right scan -- it never
# backtracks. Put before objects.a/linkLibs.rsp, these three archives
# were being scanned before anything had asked for their symbols and so
# were silently contributing nothing, regardless of being present on
# the command line at all.
# CMAKE_<LANG>_STANDARD_LIBRARIES, by contrast, is appended after
# everything else (it's what -lkernel32 -luser32 etc. already use below).
set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -lssp -lucrt -lucrtbase")
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lssp -lucrt -lucrtbase")

# Ubuntu's mingw-w64-x86-64 toolchain targets the classic msvcrt-default
# triple: it ships libucrt.a/libucrtbase.a as *additional* opt-in import
# libraries, but g++'s own driver spec still unconditionally appends
# -lmsvcrt at the very end regardless (there's no UCRT-native triple
# here the way MSYS2 packages one separately). msvcrt.a and ucrt.a both
# provide a handful of overlapping legacy wide-char functions (seen so
# far: wcsrtombs, mbsrtowcs), so linking both -- unavoidable without a
# UCRT-native toolchain -- trips ld's multiple-definition check even
# though the two implementations are functionally interchangeable for
# these. This is a global linker policy, not a library reference, so
# unlike the -l flags above it doesn't need CMAKE_*_STANDARD_LIBRARIES
# positioning; CMAKE_EXE_LINKER_FLAGS (processed early, but that's fine
# for a policy flag) is fine.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--allow-multiple-definition")

# Unlike the llvm-mingw toolchain files (which rely solely on PATH), point
# FIND_ROOT_PATH at Ubuntu's mingw-w64 sysroot so that find_library()/
# find_path() calls in CMakeLists.txt can't accidentally pick up a
# same-named host library instead of building one from source.
set(CMAKE_FIND_ROOT_PATH /usr/${triple})

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
