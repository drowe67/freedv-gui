add_library(fdv_reporting STATIC
    pskreporter.cpp
    pskreporter.h
)

target_include_directories(fdv_reporting PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)