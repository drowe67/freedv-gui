if(WIN32)
    set(UTIL_WIN32_FILES logging/libfmemopen.c Win32COMObject.cpp)
endif(WIN32)

if(LINUX)
    set(UTIL_LINUX_FILES timespec.c)
endif(LINUX)

if(APPLE)
    set(THREADED_OBJECT_FILES ThreadedObject.mm)
else(APPLE)
    set(THREADED_OBJECT_FILES ThreadedObject.cpp)
endif(APPLE)

add_library(fdv_util STATIC
    Semaphore.cpp
    SocketIoClient.cpp
    ${THREADED_OBJECT_FILES}
    ThreadedTimer.cpp
    TcpConnectionHandler.cpp
    logging/ulog.c
    ${UTIL_LINUX_FILES}
    ${UTIL_WIN32_FILES}
)

target_compile_options(fdv_util PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_util yyjson fdv_os_wrapper)

if(UNITTEST)
macro(DefineUnitTest utName)
    add_executable(${utName} test/${utName}.cpp)
    target_link_libraries(${utName} PRIVATE fdv_util)

    add_test(NAME util_${utName} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${utName})
endmacro()

DefineUnitTest(ThreadedTimerTest)
endif(UNITTEST)

