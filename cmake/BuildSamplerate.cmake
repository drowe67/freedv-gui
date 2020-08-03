set(SAMPLERATE_TARBALL "libsamplerate-0.1.9")

set(PATCH_COMMAND patch -p0 < ${CMAKE_BINARY_DIR}/../patch-samplerate.diff)
if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --target=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-sndfile --disable-fftw)
elseif(APPLE)
    set(CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9\ -arch\ arm64e\ -arch\ x86_64)
else()
    set(CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist)
endif()

include(ExternalProject)
ExternalProject_Add(samplerate
    URL http://www.mega-nerd.com/SRC/${SAMPLERATE_TARBALL}.tar.gz 
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    PATCH_COMMAND ${PATCH_COMMAND}
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)
if(WIN32)
    set(SAMPLERATE_LIBRARIES
        ${CMAKE_BINARY_DIR}/external/dist/lib/libsamplerate.a)
else(WIN32)
    set(SAMPLERATE_LIBRARIES
        ${CMAKE_BINARY_DIR}/external/dist/lib/libsamplerate.a)
endif(WIN32)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)
list(APPEND FREEDV_LINK_LIBS ${SAMPLERATE_LIBRARIES})
list(APPEND FREEDV_STATIC_DEPS samplerate)
