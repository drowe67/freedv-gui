if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --target=${HOST} --without-cxx-binding --enable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist --without-libusb CFLAGS=-g\ -O2\ -fstack-protector CXXFLAGS=-g\ -O2\ -fstack-protector)
    set(HAMLIB_PATCH_CMD patch -p1 < ${CMAKE_SOURCE_DIR}/cmake/hamlib-windows.patch)
else(MINGW AND CMAKE_CROSSCOMPILING)
    set(HAMLIB_PATCH_CMD "")
if(APPLE)
if(BUILD_OSX_UNIVERSAL)
    set(CONFIGURE_COMMAND ./configure --enable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist --without-cxx-binding --without-libusb CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9\ -arch\ x86_64\ -arch\ arm64 CXXFLAGS=-g\ -O2\ -mmacosx-version-min=10.9\ -arch\ x86_64\ -arch\ arm64)
else()
    set(CONFIGURE_COMMAND ./configure --enable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist --without-cxx-binding --without-libusb CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9 CXXFLAGS=-g\ -O2\ -mmacosx-version-min=10.9)
endif(BUILD_OSX_UNIVERSAL)
else()
    set(CONFIGURE_COMMAND ./configure --enable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist --without-cxx-binding --without-libusb CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})
endif()
endif(MINGW AND CMAKE_CROSSCOMPILING)

include(ExternalProject)
ExternalProject_Add(build_hamlib
    URL https://github.com/Hamlib/Hamlib/archive/refs/tags/4.6.5.zip
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    PATCH_COMMAND ${HAMLIB_PATCH_CMD}
    CONFIGURE_COMMAND ./bootstrap && ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)

ExternalProject_Get_Property(build_hamlib BINARY_DIR)
ExternalProject_Get_Property(build_hamlib SOURCE_DIR)
add_library(hamlib SHARED IMPORTED)
add_dependencies(hamlib build_hamlib)

set_target_properties(hamlib PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/external/dist/lib/libhamlib${CMAKE_SHARED_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${CMAKE_BINARY_DIR}/external/dist/lib/libhamlib${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)

message(STATUS "hamlib path" "${CMAKE_BINARY_DIR}/external/dist/lib/libhamlib${CMAKE_IMPORT_LIBRARY_SUFFIX}")

add_dependencies(hamlib build_hamlib)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)

list(APPEND FREEDV_LINK_LIBS hamlib)    
list(APPEND FREEDV_STATIC_DEPS hamlib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
set(HAMLIB_ADD_DEPENDENCY TRUE)
