set(SNDFILE_TARBALL "libsndfile-1.0.28")

if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND autoreconf -i && ./configure --host=${HOST} --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-external-libs --disable-shared --disable-sqlite)
elseif(APPLE)
if(BUILD_OSX_UNIVERSAL)
    set(CONFIGURE_COMMAND autoreconf -i && ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-shared --disable-external-libs CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9\ -arch\ x86_64\ -arch\ arm64 LDFLAGS=-arch\ x86_64\ -arch\ arm64)
else()
    set(CONFIGURE_COMMAND autoreconf -i && ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-shared --disable-external-libs CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9)
endif(BUILD_OSX_UNIVERSAL)
else()
    set(CONFIGURE_COMMAND autoreconf -i && ./configure --prefix=${CMAKE_BINARY_DIR}/external/dist --disable-external-libs --disable-shared --disable-external-libs)
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
