if(CMAKE_CROSSCOMPILING)
    set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DPython3_ROOT_DIR=${Python3_ROOT_DIR} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
elseif(APPLE)
    set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DPython3_ROOT_DIR=${Python3_ROOT_DIR})
endif()

if(BUILD_OSX_UNIVERSAL)
    set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DBUILD_OSX_UNIVERSAL=1)
endif(BUILD_OSX_UNIVERSAL)

set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DOPUS_URL=https://github.com/xiph/opus/archive/940d4e5af64351ca8ba8390df3f555484c567fbb.zip)

include(ExternalProject)
ExternalProject_Add(build_rade_integ
   SOURCE_DIR rade_integ_src
   BINARY_DIR rade_integ_build
   GIT_REPOSITORY https://github.com/drowe67/radae.git
   GIT_TAG ms-disable-python-gc
   CMAKE_ARGS ${RADE_CMAKE_ARGS}
   #CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
   INSTALL_COMMAND ""
   EXCLUDE_FROM_ALL TRUE
)

ExternalProject_Get_Property(build_rade_integ BINARY_DIR)
ExternalProject_Get_Property(build_rade_integ SOURCE_DIR)
add_library(rade_integ SHARED IMPORTED)
add_dependencies(rade_integ build_rade_integ)
include_directories(${SOURCE_DIR}/src)

set_target_properties(rade_integ PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/src/librade${CMAKE_SHARED_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/src/librade${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)
list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/src)
set(rade_BINARY_DIR ${BINARY_DIR})

add_library(opus_integ STATIC IMPORTED)
add_dependencies(opus_integ build_rade_integ)
set(FARGAN_INTEG_ARM_CONFIG_H_FILE "${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm/config.h")
set(FARGAN_INTEG_X86_CONFIG_H_FILE "${BINARY_DIR}/build_opus_x86-prefix/src/build_opus_x86/config.h")

if(APPLE AND BUILD_OSX_UNIVERSAL)
include_directories(
    ${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm
    ${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm/dnn
    ${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm/celt
    ${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm/silk
    ${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm/include)
set_target_properties(opus_integ PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

set(FARGAN_INTEG_CONFIG_H_FILE "${BINARY_DIR}/build_opus_arm-prefix/src/build_opus_arm/config.h")
else(APPLE AND BUILD_OSX_UNIVERSAL)
include_directories(
    ${BINARY_DIR}/build_opus-prefix/src/build_opus
    ${BINARY_DIR}/build_opus-prefix/src/build_opus/dnn
    ${BINARY_DIR}/build_opus-prefix/src/build_opus/celt
    ${BINARY_DIR}/build_opus-prefix/src/build_opus/silk
    ${BINARY_DIR}/build_opus-prefix/src/build_opus/include)
set_target_properties(opus_integ PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/build_opus-prefix/src/build_opus/.libs/libopus${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
set(FARGAN_INTEG_CONFIG_H_FILE "${BINARY_DIR}/build_opus-prefix/src/build_opus/config.h")
set(FARGAN_INTEG_ARM_CONFIG_H_FILE "${FARGAN_CONFIG_H_FILE}")
set(FARGAN_INTEG_X86_CONFIG_H_FILE "${FARGAN_CONFIG_H_FILE}")
endif(APPLE AND BUILD_OSX_UNIVERSAL)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/fargan_config_integ.h.in" "${CMAKE_CURRENT_BINARY_DIR}/fargan_config_integ.h")

