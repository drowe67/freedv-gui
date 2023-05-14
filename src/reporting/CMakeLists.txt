set(SIO_CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF)
if(CMAKE_CROSSCOMPILING)
    set(SIO_CMAKE_ARGS ${SIO_CMAKE_ARGS} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

include(FetchContent)
FetchContent_Declare(
    sioclient
    GIT_REPOSITORY https://github.com/socketio/socket.io-client-cpp.git
    GIT_TAG master)
FetchContent_GetProperties(sioclient)
if(NOT sioclient_POPULATED)
  FetchContent_Populate(sioclient)
  add_subdirectory(${sioclient_SOURCE_DIR} ${sioclient_BINARY_DIR} EXCLUDE_FROM_ALL)
  list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${sioclient_BINARY_DIR})
endif()

add_library(fdv_reporting STATIC
    pskreporter.cpp
    pskreporter.h
    FreeDVReporter.cpp
    FreeDVReporter.h
)

target_link_libraries(fdv_reporting PUBLIC sioclient)
target_include_directories(fdv_reporting PUBLIC ${sioclient_SOURCE_DIR}/src)
