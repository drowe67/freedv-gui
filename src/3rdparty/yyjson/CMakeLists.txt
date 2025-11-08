add_library(yyjson STATIC yyjson.c)
target_include_directories(yyjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
set_source_files_properties(yyjson.c PROPERTIES
    SKIP_LINTING ON
)
endif (CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
