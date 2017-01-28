set(SAMPLERATE_TARBALL "libsamplerate-0.1.8")

if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND ./configure --build=${HOST} --host=${HOST} --target=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-sndfile --disable-fftw)
else()
    set(CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist)
endif()

include(ExternalProject)
ExternalProject_Add(samplerate
    URL http://www.mega-nerd.com/SRC/${SAMPLERATE_TARBALL}.tar.gz 
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
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
