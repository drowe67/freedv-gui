add_library(agc STATIC agc.c)
target_include_directories(agc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
