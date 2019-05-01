#set(SPEEXDSP_CMAKE_ARGS -DBUILD_SHARED_LIBS=FALSE -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/dist)
set(SPEEXDSP_CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/dist)

if(USE_STATIC_SPEEXDSP)
    list(APPEND SPEEXDSP_CMAKE_ARGS 
        -DSPEEXDSP_LIBRARIES=${CMAKE_BINARY_DIR}/external/dist/lib/libspeexdsp.a
        -DSPEEXDSP_INCLUDE_DIR=${CMAKE_BINARY_DIR}/external/dist/include)
endif()

set(CODEC2_CMAKE_ARGS -DUNITTEST=FALSE)

if(CMAKE_CROSSCOMPILING)
    set(CODEC2_CMAKE_ARGS ${CODEC2_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

# Bootstrap build for lpcnetfreedv library.
include(ExternalProject)
ExternalProject_Add(build_codec2_lpcnet
   SOURCE_DIR codec2_src
   BINARY_DIR codec2_build
   GIT_REPOSITORY https://github.com/drowe67/codec2.git
   GIT_TAG origin/brad-2020
   CMAKE_ARGS ${CODEC2_CMAKE_ARGS} ${SPEEXDSP_CMAKE_ARGS} 
   CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=10.7
   INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_codec2_lpcnet BINARY_DIR)
ExternalProject_Get_Property(build_codec2_lpcnet SOURCE_DIR)
include_directories(${SOURCE_DIR}/src ${BINARY_DIR})


# Bootstrap lpcnetfreedv library
include(cmake/BuildLPCNet.cmake)
add_dependencies(build_lpcnetfreedv build_codec2_lpcnet)


# Build final codec2 library with lpcnetfreedv
set(CODEC2_CMAKE_ARGS ${CODEC2_CMAKE_ARGS} -DLPCNET_BUILD_DIR=${CMAKE_BINARY_DIR}/LPCNet_build)
include(ExternalProject)
ExternalProject_Add(build_codec2
   SOURCE_DIR codec2_src
   BINARY_DIR codec2_build
   DOWNLOAD_COMMAND ""
   CMAKE_ARGS ${CODEC2_CMAKE_ARGS} ${SPEEXDSP_CMAKE_ARGS} 
   CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=10.7
   INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_codec2 BINARY_DIR)
ExternalProject_Get_Property(build_codec2 SOURCE_DIR)
add_library(codec2 SHARED IMPORTED)
set_target_properties(codec2 PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/src/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/src/libcodec2${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)
add_dependencies(build_codec2 build_lpcnetfreedv)
