if(WIN32)
    set(UTIL_WIN32_FILES logging/libfmemopen.c Win32COMObject.cpp)
endif(WIN32)

if(LINUX)
    set(UTIL_LINUX_FILES timespec.c)
endif(LINUX)

add_library(fdv_util STATIC
    SocketIoClient.cpp
    ThreadedObject.cpp
    ThreadedTimer.cpp
    TcpConnectionHandler.cpp
    logging/ulog.c
    ${UTIL_LINUX_FILES}
    ${UTIL_WIN32_FILES}
)
