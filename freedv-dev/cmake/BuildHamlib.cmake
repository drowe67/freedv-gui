set(HAMLIB_TARBALL "hamlib-1.2.15.3")

include(ExternalProject)
ExternalProject_Add(hamlib
    URL http://downloads.sourceforge.net/hamlib/${HAMLIB_TARBALL}.tar.gz
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)
if(WIN32)
    set(HAMLIB_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/portaudio.lib)
else(WIN32)
    set(HAMLIB_LIBRARIES
    )
endif(WIN32)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)
list(APPEND FREEDV_LINK_LIBS ${HAMLIB_LIBRARIES})
list(APPEND FREEDV_STATIC_DEPS hamlib)
