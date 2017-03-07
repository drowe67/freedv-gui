set(SPEEXDSP_TARBALL "speexdsp-1.2rc3.tar.gz")

if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-examples)
else()
    if(APPLE)
        set(CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/../configure_speexdsp_osx.sh ${CMAKE_BINARY_DIR}/external/dist)
    else()
        set(CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-examples)
    endif()
endif()

include(ExternalProject)
ExternalProject_Add(speex
   URL http://downloads.xiph.org/releases/speex/${SPEEXDSP_TARBALL}
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
if(USE_STATIC_CODEC2)
    add_dependencies(codec2 speex)
endif()
