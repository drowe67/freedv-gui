if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
    set(CONFIGURE_COMMAND ./autogen.sh && ./configure --host=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-examples)
elseif(APPLE)
if(BUILD_OSX_UNIVERSAL)
    set(CONFIGURE_COMMAND ./autogen.sh && ${CMAKE_BINARY_DIR}/../configure_speexdsp_osx_universal.sh ${CMAKE_BINARY_DIR}/external/dist)
else()
    set(CONFIGURE_COMMAND ./autogen.sh && ${CMAKE_BINARY_DIR}/../configure_speexdsp_osx.sh ${CMAKE_BINARY_DIR}/external/dist)
endif(BUILD_OSX_UNIVERSAL)
else()
    set(CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-examples)
endif()

include(ExternalProject)
ExternalProject_Add(speex
   GIT_REPOSITORY https://gitlab.xiph.org/xiph/speexdsp.git
   GIT_TAG origin/master
   BUILD_IN_SOURCE 1
   INSTALL_DIR external/dist
   CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
   BUILD_COMMAND $(MAKE)
   INSTALL_COMMAND $(MAKE) install
)

set(SPEEXDSP_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/libspeexdsp.a)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)
list(APPEND FREEDV_LINK_LIBS ${SPEEXDSP_LIBRARIES})
list(APPEND FREEDV_STATIC_DEPS speex)
