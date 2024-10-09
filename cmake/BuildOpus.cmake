message(STATUS "Will build opus with FARGAN")

set(CONFIGURE_COMMAND ./autogen.sh && ./configure --enable-dred --disable-shared --disable-doc --disable-extra-programs)

if (CMAKE_CROSSCOMPILING)
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=${CMAKE_C_COMPILER_TARGET} --target=${CMAKE_C_COMPILER_TARGET})
endif (CMAKE_CROSSCOMPILING)

set(OPUS_URL https://gitlab.xiph.org/xiph/opus/-/archive/main/opus-main.tar.gz)

include(ExternalProject)
if(APPLE AND BUILD_OSX_UNIVERSAL)
# Opus ./configure doesn't behave properly when built as a universal binary;
# build it twice and use lipo to create a universal libopus.a instead.
ExternalProject_Add(build_opus_x86
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=x86_64-apple-darwin --target=x86_64-apple-darwin CFLAGS=-arch\ x86_64\ -O2
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)
ExternalProject_Add(build_opus_arm
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=aarch64-apple-darwin --target=aarch64-apple-darwin CFLAGS=-arch\ arm64\ -O2
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)

ExternalProject_Get_Property(build_opus_arm BINARY_DIR)
ExternalProject_Get_Property(build_opus_arm SOURCE_DIR)
set(OPUS_ARM_BINARY_DIR ${BINARY_DIR})
ExternalProject_Get_Property(build_opus_x86 BINARY_DIR)
set(OPUS_X86_BINARY_DIR ${BINARY_DIR})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}
    COMMAND lipo ${OPUS_ARM_BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX} ${OPUS_X86_BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX} -output ${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX} -create
    DEPENDS build_opus_arm build_opus_x86)

add_custom_target(
    libopus.a
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX})

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include)

add_library(opus STATIC IMPORTED)
add_dependencies(opus libopus.a)
set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

else(APPLE AND BUILD_OSX_UNIVERSAL)
ExternalProject_Add(build_opus
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
    URL ${OPUS_URL}
)

ExternalProject_Get_Property(build_opus BINARY_DIR)
ExternalProject_Get_Property(build_opus SOURCE_DIR)
add_library(opus STATIC IMPORTED)
add_dependencies(opus build_opus)

set_target_properties(opus PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

include_directories(${SOURCE_DIR}/dnn ${SOURCE_DIR}/celt ${SOURCE_DIR}/include)
list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/.libs)
endif(APPLE AND BUILD_OSX_UNIVERSAL)
