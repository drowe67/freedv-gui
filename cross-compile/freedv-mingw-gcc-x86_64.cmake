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

# Ubuntu's gcc bakes in _FORTIFY_SOURCE hardening by default (even when
# cross-compiling), so calls like memcpy() with a compile-time-known
# destination size get rewritten to __memcpy_chk() etc. On Linux those
# come from glibc; mingw-w64 has no libc of its own providing them, so
# without this they fail to link:
#   undefined reference to `__memcpy_chk'
# libssp.a (shipped by gcc-mingw-w64-x86-64) provides the fortify-source
# runtime for mingw targets.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lssp")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lssp")

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
