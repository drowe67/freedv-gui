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

# Opt into UCRT-only CRT additions (e.g. C11 timespec_get(), used by
# freedv-backend's logging code) -- without this, mingw-w64's headers
# don't even declare them, regardless of what's linked.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_UCRT")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_UCRT")

# The extra runtime libraries needed to satisfy the above (-lssp for
# _FORTIFY_SOURCE's __memcpy_chk & co, -lucrt/-lucrtbase for UCRT-only
# additions like _timespec64_get) can't be set here as CMAKE_<LANG>_
# STANDARD_LIBRARIES: CMake's Windows-GNU platform file unconditionally
# (re)populates that variable (e.g. with "-lkernel32 -luser32 ...")
# during language enablement, which runs *after* the toolchain file and
# clobbers whatever it set here. They're set instead in the top-level
# CMakeLists.txt (after project()) and forwarded explicitly to the
# nested build_rade ExternalProject in freedv-backend's BuildRADE.cmake.

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

# Statically link the C++ runtime (libstdc++, libgcc, libwinpthread).
# By default these are separate DLLs, which caused a real, reproducible
# crash: freedv.exe segfaulted (STATUS_ACCESS_VIOLATION writing to
# address 0x8 -- a null-pointer member write) *inside libstdc++-6.dll
# itself*, confirmed from a Windows crash dump captured by CI. This is
# a known class of bug with mingw-w64's threaded C++ runtime split
# across DLLs (cross-DLL static-initialization-order/TLS issues); the
# standard fix is to statically link it into the executable instead.
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
