add_library(ujson STATIC double-conversion.cc ujson.cpp)
target_include_directories(ujson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
