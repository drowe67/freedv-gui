if(WIN32)
    set(UTIL_WIN32_FILES logging/libfmemopen.c Win32COMObject.cpp)
endif(WIN32)

if(LINUX)
    set(UTIL_LINUX_FILES timespec.c)
endif(LINUX)

if(APPLE)
    set(THREADED_OBJECT_FILES ThreadedObject.mm)
endif(APPLE)

add_library(fdv_util STATIC
    Semaphore.cpp
    SocketIoClient.cpp
    ThreadedObject.cpp
    ${THREADED_OBJECT_FILES}
    ThreadedTimer.cpp
    TcpConnectionHandler.cpp
    logging/ulog.c
    ${UTIL_LINUX_FILES}
    ${UTIL_WIN32_FILES}
)

target_link_libraries(fdv_util yyjson)
