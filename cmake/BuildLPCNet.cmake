if(CMAKE_CROSSCOMPILING)
    set(LPCNET_CMAKE_ARGS ${LPCNET_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

if(BUILD_OSX_UNIVERSAL)
    set(LPCNET_CMAKE_ARGS ${LPCNET_CMAKE_ARGS} -DBUILD_OSX_UNIVERSAL=1)
endif(BUILD_OSX_UNIVERSAL)

include(ExternalProject)
ExternalProject_Add(build_lpcnetfreedv
   SOURCE_DIR LPCNet_src
   BINARY_DIR LPCNet_build
   GIT_REPOSITORY https://github.com/drowe67/LPCNet.git
   GIT_TAG v0.5
   CMAKE_ARGS ${LPCNET_CMAKE_ARGS}
   CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
   INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_lpcnetfreedv BINARY_DIR)
ExternalProject_Get_Property(build_lpcnetfreedv SOURCE_DIR)
add_library(lpcnetfreedv SHARED IMPORTED)

set_target_properties(lpcnetfreedv PROPERTIES 
    IMPORTED_LOCATION "${BINARY_DIR}/src/liblpcnetfreedv${CMAKE_SHARED_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/src/liblpcnetfreedv${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)

include_directories(${SOURCE_DIR}/src)
