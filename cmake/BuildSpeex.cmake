if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-examples)
elseif(APPLE)
if(BUILD_OSX_UNIVERSAL)
    set(CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/../cmake/configure_speexdsp_osx_universal.sh ${CMAKE_BINARY_DIR}/external/dist)
else()
    set(CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/../cmake/configure_speexdsp_osx.sh ${CMAKE_BINARY_DIR}/external/dist)
endif(BUILD_OSX_UNIVERSAL)
else()
    set(CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-examples)
endif()

include(ExternalProject)
ExternalProject_Add(speex
   URL https://github.com/xiph/speexdsp/archive/refs/heads/master.zip
   BUILD_IN_SOURCE 1
   INSTALL_DIR external/dist
   PATCH_COMMAND ./autogen.sh
   CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
   BUILD_COMMAND $(MAKE)
   INSTALL_COMMAND $(MAKE) install
)

set(SPEEXDSP_LIBRARY ${CMAKE_BINARY_DIR}/external/dist/lib/libspeexdsp.a)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)
list(APPEND FREEDV_LINK_LIBS ${SPEEXDSP_LIBRARY})
list(APPEND FREEDV_STATIC_DEPS speex)
