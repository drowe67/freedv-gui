set(SIO_CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF)
if(CMAKE_CROSSCOMPILING)
    set(SIO_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

include(ExternalProject)
if(APPLE)
    ExternalProject_Add(build_sioclient
       SOURCE_DIR sioclient_src
       BINARY_DIR sioclient_build
       GIT_REPOSITORY https://github.com/socketio/socket.io-client-cpp.git
       GIT_TAG master
       CMAKE_ARGS ${SIO_CMAKE_ARGS}
       CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET} -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
       INSTALL_COMMAND ""
    )
else(APPLE)
    ExternalProject_Add(build_sioclient
       SOURCE_DIR sioclient_src
       BINARY_DIR sioclient_build
       GIT_REPOSITORY https://github.com/socketio/socket.io-client-cpp.git
       GIT_TAG master
       CMAKE_ARGS ${SIO_CMAKE_ARGS}
       INSTALL_COMMAND ""
    )
endif(APPLE)

ExternalProject_Get_Property(build_sioclient BINARY_DIR)
ExternalProject_Get_Property(build_sioclient SOURCE_DIR)
add_library(sioclient SHARED IMPORTED)

set_target_properties(sioclient PROPERTIES 
    IMPORTED_LOCATION "${BINARY_DIR}/libsioclient${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/libsioclient${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)

add_library(fdv_reporting STATIC
    pskreporter.cpp
    pskreporter.h
    FreeDVReporter.cpp
    FreeDVReporter.h
)

add_dependencies(fdv_reporting build_sioclient)
target_link_libraries(fdv_reporting PUBLIC sioclient)
target_include_directories(fdv_reporting PUBLIC ${SOURCE_DIR}/src)