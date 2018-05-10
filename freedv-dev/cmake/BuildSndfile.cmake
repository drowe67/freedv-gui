set(SNDFILE_TARBALL "libsndfile-1.0.25")

if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-external-libs --disable-shared --disable-sqlite)
else()
    set(CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-external-libs --disable-shared --disable-external-libs)
endif()

include(ExternalProject)
ExternalProject_Add(sndfile
    URL http://www.mega-nerd.com/libsndfile/files/${SNDFILE_TARBALL}.tar.gz
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE) V=1
    INSTALL_COMMAND $(MAKE) install
)
if(MINGW)
    set(SNDFILE_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/libsndfile.a)
else()
    set(SNDFILE_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/libsndfile.a)
endif()

include_directories(${CMAKE_BINARY_DIR}/external/dist/include)
list(APPEND FREEDV_LINK_LIBS ${SNDFILE_LIBRARIES})
list(APPEND FREEDV_STATIC_DEPS sndfile)
