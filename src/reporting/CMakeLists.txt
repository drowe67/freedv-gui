set(SIO_CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF)
set(LIBRESSL_CMAKE_ARGS -DLIBRESSL_APPS=OFF -DLIBRESSL_TESTS=OFF -DBUILD_SHARED_LIBS=ON)
if(CMAKE_CROSSCOMPILING)
    set(SIO_CMAKE_ARGS ${SIO_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    set(LIBRESSL_CMAKE_ARGS ${LIBRESSL_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

# We need to set up OpenSSL/LibreSSL as we don't want to use an insecure connection
# to the reporting server. 
if(WIN32)
    # On Windows, it's very unlikely we'll have pre-built binaries available.
    # Set the project up to build it from source. 
    # XXX - This may take quite a long time. Consider maintaining DLLs separately
    # (especially for ARM) and download those instead if possible.
    ExternalProject_Add(build_libressl
       SOURCE_DIR libressl_src
       BINARY_DIR libressl_build
       URL https://github.com/libressl/portable/archive/refs/tags/v3.7.2.tar.gz
       CMAKE_ARGS ${LIBRESSL_CMAKE_ARGS}
       PATCH_COMMAND "./autogen.sh"
       INSTALL_COMMAND ""
    )

    ExternalProject_Get_Property(build_libressl BINARY_DIR)
    ExternalProject_Get_Property(build_libressl SOURCE_DIR)
    add_library(crypto SHARED IMPORTED)
    add_library(ssl SHARED IMPORTED)
    add_library(tls SHARED IMPORTED)

    set_target_properties(crypto PROPERTIES 
        IMPORTED_LOCATION "${BINARY_DIR}/crypto/libcrypto-50${CMAKE_SHARED_LIBRARY_SUFFIX}"
        IMPORTED_IMPLIB   "${BINARY_DIR}/crypto/libcrypto-50${CMAKE_IMPORT_LIBRARY_SUFFIX}"
    )
    set_target_properties(ssl PROPERTIES 
        IMPORTED_LOCATION "${BINARY_DIR}/ssl/libssl-53${CMAKE_SHARED_LIBRARY_SUFFIX}"
        IMPORTED_IMPLIB   "${BINARY_DIR}/ssl/libssl-53${CMAKE_IMPORT_LIBRARY_SUFFIX}"
    )
    set_target_properties(tls PROPERTIES 
        IMPORTED_LOCATION "${BINARY_DIR}/tls/libtls-26${CMAKE_SHARED_LIBRARY_SUFFIX}"
        IMPORTED_IMPLIB   "${BINARY_DIR}/tls/libtls-26${CMAKE_IMPORT_LIBRARY_SUFFIX}"
    )

    # Alias OpenSSL to LibreSSL
    #add_library(OpenSSL ALIAS LibreSSL)
    add_library(OpenSSL::Crypto ALIAS crypto)
    add_library(OpenSSL::SSL ALIAS ssl)
    add_library(OpenSSL::TLS ALIAS tls)

    set(OPENSSL_FOUND TRUE)
    set(OPENSSL_INCLUDE_DIR ${SOURCE_DIR}/include)
    set(OPENSSL_CRYPTO_LIBRARY ${BINARY_DIR}/crypto)
    set(OPENSSL_SSL_LIBRARY ${BINARY_DIR}/ssl)
    set(OPENSSL_TLS_LIBRARY ${BINARY_DIR}/tls)
    list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/crypto)
    list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/ssl)
    list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/tls)
else(WIN32)
    find_package(OpenSSL REQUIRED)
endif(WIN32)

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
add_library(sioclient_tls STATIC IMPORTED)

set_target_properties(sioclient_tls PROPERTIES 
    IMPORTED_LOCATION "${BINARY_DIR}/libsioclient${CMAKE_STATIC_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/libsioclient${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)

if(WIN32)
    add_dependencies(build_sioclient build_libressl)
endif(WIN32)

add_library(fdv_reporting STATIC
    pskreporter.cpp
    pskreporter.h
    FreeDVReporter.cpp
    FreeDVReporter.h
)

add_dependencies(fdv_reporting build_sioclient)
target_link_libraries(fdv_reporting PUBLIC sioclient_tls crypto ssl)
target_include_directories(fdv_reporting PUBLIC ${SOURCE_DIR}/src)
