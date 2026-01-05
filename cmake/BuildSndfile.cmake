# Ensure that MP3 support is built into libsndfile
include(cmake/BuildMpg123.cmake)
include(cmake/BuildLame.cmake)

set(SNDFILE_CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/dist -DBUILD_SHARED_LIBS=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DENABLE_CPACK=OFF -DENABLE_EXTERNAL_LIBS=OFF -Dmp3lame_ROOT=${CMAKE_BINARY_DIR}/external/dist -DLAME_ROOT=${CMAKE_BINARY_DIR}/external/dist -Dmpg123_ROOT=${CMAKE_BINARY_DIR}/external/dist)
if(CMAKE_CROSSCOMPILING)
    set(SNDFILE_CMAKE_ARGS ${SNDFILE_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

include(ExternalProject)
ExternalProject_Add(sndfile
    URL https://github.com/libsndfile/libsndfile/archive/refs/tags/1.2.2.tar.gz
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    PATCH_COMMAND patch -p1 < ${CMAKE_SOURCE_DIR}/cmake/sndfile-cmake.patch
    CMAKE_ARGS ${SNDFILE_CMAKE_ARGS}
    CMAKE_CACHE_ARGS -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET} -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)
if(MINGW)
    set(SNDFILE_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/libsndfile.a)
else()
    set(SNDFILE_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/libsndfile.a)
endif()

include_directories(${CMAKE_BINARY_DIR}/external/dist/include)
list(APPEND FREEDV_LINK_LIBS mp3lame mpg123 ${SNDFILE_LIBRARIES})
list(APPEND FREEDV_STATIC_DEPS mpg123 mp3lame sndfile)

add_dependencies(sndfile mpg123 mp3lame)
