# If we're cross-compiling then we need to set the target host manually.
if(MINGW AND CMAKE_CROSSCOMPILING)
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(HOST x86_64-w64-mingw32)
    else()
        set(HOST i686-w64-mingw32)
    endif()
endif()
