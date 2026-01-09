set(CONFIGURE_COMMAND ./configure --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)

if (CMAKE_CROSSCOMPILING)
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=${CMAKE_C_COMPILER_TARGET} --target=${CMAKE_C_COMPILER_TARGET})
endif (CMAKE_CROSSCOMPILING)

set(LAME_URL https://download.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz)
set(LAME_HASH SHA512=0844b9eadb4aacf8000444621451277de365041cc1d97b7f7a589da0b7a23899310afd4e4d81114b9912aa97832621d20588034715573d417b2923948c08634b)

include(ExternalProject)
if(APPLE AND BUILD_OSX_UNIVERSAL)
# mp3lame doesn't behave properly when trying to build a universal binary;
# build it twice and use lipo to create a universal library instead.
ExternalProject_Add(build_mp3lame_x86
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=x86_64-apple-darwin --target=x86_64-apple-darwin CFLAGS=-arch\ x86_64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND $(MAKE) && $(MAKE) install
    INSTALL_COMMAND ""
    URL ${LAME_URL}
    URL_HASH ${LAME_HASH}
)
ExternalProject_Add(build_mp3lame_arm
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=aarch64-apple-darwin --target=aarch64-apple-darwin CFLAGS=-arch\ arm64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND $(MAKE) && $(MAKE) install
    INSTALL_COMMAND ""
    URL ${LAME_URL}
    URL_HASH ${LAME_HASH}
)

ExternalProject_Get_Property(build_mp3lame_arm SOURCE_DIR)
set(LAME_ARM_BINARY_DIR ${SOURCE_DIR})
ExternalProject_Get_Property(build_mp3lame_x86 SOURCE_DIR)
set(LAME_X86_BINARY_DIR ${SOURCE_DIR})

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX}
    COMMAND lipo ${LAME_ARM_BINARY_DIR}/libmp3lame/.libs/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX} ${LAME_X86_BINARY_DIR}/libmp3lame/.libs/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX} -output ${CMAKE_BINARY_DIR}/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX} -create && cp ${CMAKE_BINARY_DIR}/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_BINARY_DIR}/external/dist/lib
    DEPENDS build_mp3lame_arm build_mp3lame_x86)

add_custom_target(
    libmp3lame.a
    DEPENDS ${CMAKE_BINARY_DIR}/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(mp3lame STATIC IMPORTED)
add_dependencies(mp3lame libmp3lame.a)
set_target_properties(mp3lame PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/external/dist/lib/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

else(APPLE AND BUILD_OSX_UNIVERSAL)
ExternalProject_Add(build_mp3lame
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE) && $(MAKE) install
    URL ${LAME_URL}
    URL_HASH ${LAME_HASH}
)

add_library(mp3lame STATIC IMPORTED)
add_dependencies(mp3lame build_mp3lame)

set_target_properties(mp3lame PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/external/dist/lib/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${CMAKE_BINARY_DIR}/external/dist/lib/libmp3lame${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

endif(APPLE AND BUILD_OSX_UNIVERSAL)
