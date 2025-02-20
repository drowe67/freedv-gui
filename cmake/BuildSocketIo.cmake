set(SIO_CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF)
if(CMAKE_CROSSCOMPILING)
    set(SIO_CMAKE_ARGS ${SIO_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

set(SIOCLIENT_ADD_DEPENDENCY ON)

include(ExternalProject)
if(APPLE)
    ExternalProject_Add(build_sioclient
       SOURCE_DIR sioclient_src
       BINARY_DIR sioclient_build
       URL ${CMAKE_SOURCE_DIR}/src/3rdparty/socket.io-client-cpp.tar.gz
       CMAKE_ARGS ${SIO_CMAKE_ARGS}
       CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET} -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
       INSTALL_COMMAND ""
    )
else(APPLE)
    ExternalProject_Add(build_sioclient
       SOURCE_DIR sioclient_src
       BINARY_DIR sioclient_build
       URL ${CMAKE_SOURCE_DIR}/src/3rdparty/socket.io-client-cpp.tar.gz
       CMAKE_ARGS ${SIO_CMAKE_ARGS}
       INSTALL_COMMAND ""
    )
endif(APPLE)

ExternalProject_Get_Property(build_sioclient BINARY_DIR)
ExternalProject_Get_Property(build_sioclient SOURCE_DIR)
add_library(sioclient STATIC IMPORTED)
include_directories(${SOURCE_DIR}/src)
list(APPEND FREEDV_LINK_LIBS sioclient)

add_dependencies(sioclient build_sioclient)

set_target_properties(sioclient PROPERTIES 
    IMPORTED_LOCATION "${BINARY_DIR}/libsioclient${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/libsioclient${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)
