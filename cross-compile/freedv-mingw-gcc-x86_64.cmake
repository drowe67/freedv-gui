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

# Deliberately NOT defining -D_UCRT here (see below for why).
#
# -lssp (for _FORTIFY_SOURCE's __memcpy_chk & co) can't be set here as
# CMAKE_<LANG>_STANDARD_LIBRARIES: CMake's Windows-GNU platform file
# unconditionally (re)populates that variable (e.g. with "-lkernel32
# -luser32 ...") during language enablement, which runs *after* the
# toolchain file and clobbers whatever it set here. It's set instead in
# the top-level CMakeLists.txt (after project()) and forwarded
# explicitly to the nested build_rade ExternalProject in
# freedv-backend's BuildRADE.cmake.

# Ubuntu's mingw-w64-x86-64 toolchain targets the classic msvcrt-default
# triple, and with -D_UCRT no longer defined above, msvcrt.a is now the
# *only* CRT linked in -- no more overlapping-symbol situation with
# ucrt.a/ucrtbase.a to paper over with -Wl,--allow-multiple-definition.
# (That overlap used to trip ld's multiple-definition check on a
# handful of wide-char functions like wcsrtombs/mbsrtowcs when both
# were linked; it can no longer happen now that only one CRT is
# involved, so the flag was removed rather than left in place unused.)

# Statically link the C++ runtime (libstdc++, libgcc, libwinpthread).
# By default these are separate DLLs. This was originally added to fix
# what looked like a cross-DLL static-initialization-order/TLS bug
# (freedv.exe segfaulted, STATUS_ACCESS_VIOLATION writing to address
# 0x8, inside libstdc++-6.dll itself, per a CI crash dump) -- but that
# turned out to be a symptom, not the cause: the real bug was mixing
# UCRT and classic-msvcrt CRT internals (see the removed -D_UCRT above
# and freedv-backend's clock_gettime() fix), which corrupted an
# indirect call and crashed at whatever code happened to be next,
# statically linked or not. Kept anyway since it's still a real class
# of risk to avoid for a threaded C++ runtime split across DLLs, even
# though it wasn't the actual fix for the crash that motivated it.
# librade.dll/libhamlib-4.dll are pure C and don't use libstdc++, so
# this doesn't risk two coexisting C++ runtime instances -- it's
# limited to CMAKE_EXE_LINKER_FLAGS for that reason (not
# CMAKE_SHARED_LINKER_FLAGS, which would affect building those DLLs
# too, if freedv-gui's own build ever produced a shared library of its
# own via this same toolchain).
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")

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
