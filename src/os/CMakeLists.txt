if(APPLE)
    set(OS_WRAP_FILES osx_interface.mm)
else(APPLE)
    set(OS_WRAP_FILES osx_stubs.cpp)
endif(APPLE)

add_library(fdv_os_wrapper STATIC
    ${OS_WRAP_FILES}
)

target_include_directories(fdv_os_wrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)