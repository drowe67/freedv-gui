set(CODEC2_CMAKE_ARGS -DUNITTEST=FALSE)

if(CMAKE_CROSSCOMPILING)
    set(CODEC2_CMAKE_ARGS ${CODEC2_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

if(BUILD_OSX_UNIVERSAL)
    set(CODEC2_CMAKE_ARGS ${CODEC2_CMAKE_ARGS} -DBUILD_OSX_UNIVERSAL=1)
endif(BUILD_OSX_UNIVERSAL)

# Bootstrap lpcnetfreedv library
if(BOOTSTRAP_LPCNET)
    include(cmake/BuildLPCNet.cmake)
    set(CODEC2_CMAKE_ARGS ${CODEC2_CMAKE_ARGS} -DLPCNET_BUILD_DIR=${CMAKE_BINARY_DIR}/LPCNet_build)
else(BOOTSTRAP_LPCNET)
    add_definitions(-DLPCNET_DISABLED)
    set(LPCNET_DISABLED YES)
endif(BOOTSTRAP_LPCNET)

# Build codec2 library with lpcnetfreedv
include(ExternalProject)
ExternalProject_Add(build_codec2
   SOURCE_DIR codec2_src
   BINARY_DIR codec2_build
   GIT_REPOSITORY https://github.com/drowe67/codec2.git
   GIT_TAG 1.2.0
   CMAKE_ARGS ${CODEC2_CMAKE_ARGS}
   CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
   INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_codec2 BINARY_DIR)
ExternalProject_Get_Property(build_codec2 SOURCE_DIR)
add_library(codec2 SHARED IMPORTED)

set_target_properties(codec2 PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/src/libcodec2${CMAKE_SHARED_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/src/libcodec2${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)

set(CODEC2_BUILD_DIR ${BINARY_DIR})

if(BOOTSTRAP_LPCNET)
add_dependencies(build_codec2 build_lpcnetfreedv)
endif(BOOTSTRAP_LPCNET)

set(CODEC2_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/codec2_src/src ${CMAKE_BINARY_DIR}/codec2_build)
include_directories(${CODEC2_INCLUDE_DIRS})
