message(STATUS "Will build opus with FARGAN")

set(CONFIGURE_COMMAND ./autogen.sh && ./configure --enable-dred --disable-shared)

if (CMAKE_CROSSCOMPILING)
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=${CMAKE_C_COMPILER_TARGET} --target=${CMAKE_C_COMPILER_TARGET})
endif (CMAKE_CROSSCOMPILING)

include(ExternalProject)
ExternalProject_Add(build_opus
    URL https://gitlab.xiph.org/xiph/opus/-/archive/main/opus-main.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_opus BINARY_DIR)
ExternalProject_Get_Property(build_opus SOURCE_DIR)
add_library(opus SHARED IMPORTED)
add_dependencies(opus build_opus)

set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include)
list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/.libs)
