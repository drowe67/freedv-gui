message(STATUS "Will build RNNoise")

set(CONFIGURE_COMMAND ./autogen.sh && ./configure --with-pic --disable-examples --disable-doc --disable-shared)

if (CMAKE_CROSSCOMPILING)
set(CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=${CMAKE_C_COMPILER_TARGET} --target=${CMAKE_C_COMPILER_TARGET})
endif (CMAKE_CROSSCOMPILING)

set(RNNOISE_REPO https://github.com/xiph/rnnoise.git)

include(ExternalProject)
if(APPLE AND BUILD_OSX_UNIVERSAL)
# RNNoise ./configure doesn't behave properly when built as a universal binary;
# build it twice and use lipo to create a universal librnnoise.a instead.
ExternalProject_Add(build_rnnoise_x86
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --enable-x86-rtcd --host=x86_64-apple-darwin --target=x86_64-apple-darwin CFLAGS=-arch\ x86_64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
    GIT_REPOSITORY ${RNNOISE_REPO}
    GIT_TAG main
    UPDATE_DISCONNECTED 1
)
ExternalProject_Add(build_rnnoise_arm
    DOWNLOAD_EXTRACT_TIMESTAMP NO
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} --host=aarch64-apple-darwin --target=aarch64-apple-darwin CFLAGS=-arch\ arm64\ -O2\ -mmacosx-version-min=10.11
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
    GIT_REPOSITORY ${RNNOISE_REPO}
    GIT_TAG main
    UPDATE_DISCONNECTED 1
)

ExternalProject_Get_Property(build_rnnoise_arm BINARY_DIR)
ExternalProject_Get_Property(build_rnnoise_arm SOURCE_DIR)
set(RNNOISE_ARM_BINARY_DIR ${BINARY_DIR})
ExternalProject_Get_Property(build_rnnoise_x86 BINARY_DIR)
set(RNNOISE_X86_BINARY_DIR ${BINARY_DIR})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX}
    COMMAND lipo ${RNNOISE_ARM_BINARY_DIR}/.libs/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX} ${RNNOISE_X86_BINARY_DIR}/.libs/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX} -output ${CMAKE_CURRENT_BINARY_DIR}/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX} -create
    DEPENDS build_rnnoise_arm build_rnnoise_x86)

add_custom_target(
    librnnoise.a
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX})

include_directories(${SOURCE_DIR}/include)

add_library(rnnoise STATIC IMPORTED)
add_dependencies(rnnoise librnnoise.a)
set_target_properties(rnnoise PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

else(APPLE AND BUILD_OSX_UNIVERSAL)
ExternalProject_Add(build_rnnoise
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND ""
    GIT_REPOSITORY ${RNNOISE_REPO}
    GIT_TAG main
    UPDATE_DISCONNECTED 1
)

ExternalProject_Get_Property(build_rnnoise BINARY_DIR)
ExternalProject_Get_Property(build_rnnoise SOURCE_DIR)
add_library(rnnoise STATIC IMPORTED)
add_dependencies(rnnoise build_rnnoise)

set_target_properties(rnnoise PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/.libs/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/.libs/librnnoise${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

include_directories(${SOURCE_DIR}/include)
endif(APPLE AND BUILD_OSX_UNIVERSAL)
