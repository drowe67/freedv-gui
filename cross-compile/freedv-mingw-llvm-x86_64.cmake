set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(triple ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32)

set(CMAKE_C_COMPILER ${triple}-clang)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER ${triple}-clang++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(CMAKE_AR ${triple}-ar)
set(CMAKE_RANLIB ${triple}-ranlib)
set(CMAKE_RC_COMPILER ${triple}-windres)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-command-line-argument -gcodeview")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-command-line-argument -gcodeview")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--pdb=")

# For make package use.
set(CMAKE_OBJDUMP ${triple}-objdump)
set(FREEDV_USING_LLVM_MINGW 1)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search 
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
