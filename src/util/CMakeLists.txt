if(WIN32)
    set(UTIL_WIN32_FILES logging/libfmemopen.c)
endif(WIN32)

add_library(fdv_util STATIC
    SocketIoClient.cpp
    ThreadedObject.cpp
    ThreadedTimer.cpp
    TcpConnectionHandler.cpp
    logging/ulog.c
    ${UTIL_WIN32_FILES}
)
