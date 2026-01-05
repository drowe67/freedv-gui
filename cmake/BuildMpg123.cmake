set(CONFIGURE_COMMAND ./configure --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)

if (CMAKE_CROSSCOMPILING)
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=${CMAKE_C_COMPILER_TARGET} --target=${CMAKE_C_COMPILER_TARGET})
endif (CMAKE_CROSSCOMPILING)

set(MPG123_URL https://download.sourceforge.net/project/mpg123/mpg123/1.33.4/mpg123-1.33.4.tar.bz2)
set(MPG123_HASH SHA512=9b7aa93b692132da7eb8dcfef12ce91bf66bf8475af50e9c57d7b80225f96c0e74264e518e558371af1f4cf6d2afda5b3dfc844949fd747db7ac7c44d0e9f6ad)

include(ExternalProject)
if(APPLE AND BUILD_OSX_UNIVERSAL)
# mpg123 build doesn't behave properly when trying to build a universal binary;
# build it twice and use lipo to create a universal library instead.
ExternalProject_Add(build_mpg123_x86
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=x86_64-apple-darwin --target=x86_64-apple-darwin CFLAGS=-arch\ x86_64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND $(MAKE) && $(MAKE) install
    INSTALL_COMMAND ""
    URL ${MPG123_URL}
    URL_HASH ${MPG123_HASH}
)
ExternalProject_Add(build_mpg123_arm
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=aarch64-apple-darwin --target=aarch64-apple-darwin CFLAGS=-arch\ arm64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND $(MAKE) && $(MAKE) install
    INSTALL_COMMAND ""
    URL ${MPG123_URL}
    URL_HASH ${MPG123_HASH}
)

ExternalProject_Get_Property(build_mpg123_arm SOURCE_DIR)
set(MPG123_ARM_BINARY_DIR ${SOURCE_DIR})
ExternalProject_Get_Property(build_mpg123_x86 SOURCE_DIR)
set(MPG123_X86_BINARY_DIR ${SOURCE_DIR})

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX}
    COMMAND lipo ${MPG123_ARM_BINARY_DIR}/src/libmpg123/.libs/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX} ${MPG123_X86_BINARY_DIR}/src/libmpg123/.libs/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX} -output ${CMAKE_BINARY_DIR}/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX} -create && cp ${CMAKE_BINARY_DIR}/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_BINARY_DIR}/external/dist/lib
    DEPENDS build_mpg123_arm build_mpg123_x86)

add_custom_target(
    libmpg123.a
    DEPENDS ${CMAKE_BINARY_DIR}/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(mpg123 STATIC IMPORTED)
add_dependencies(mpg123 libmpg123.a)
set_target_properties(mpg123 PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/external/dist/lib/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

else(APPLE AND BUILD_OSX_UNIVERSAL)
ExternalProject_Add(build_mpg123
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE) && $(MAKE) install
    URL ${MPG123_URL}
    URL_HASH ${MPG123_HASH}
)

add_library(mpg123 STATIC IMPORTED)
add_dependencies(mpg123 build_mpg123)

set_target_properties(mpg123 PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/external/dist/lib/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${CMAKE_BINARY_DIR}/external/dist/lib/libmpg123${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

endif(APPLE AND BUILD_OSX_UNIVERSAL)

set(MPG123_ROOT ${CMAKE_BINARY_DIR}/external/dist)
