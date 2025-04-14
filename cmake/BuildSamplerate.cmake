set(SAMPLERATE_VERSION "0.2.2")

set(BUILD_TESTING OFF CACHE BOOL "Enable unit tests for libsamplerate")

include(FetchContent)
FetchContent_Declare(
    samplerate
    GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        master
)

FetchContent_GetProperties(samplerate)
if(NOT samplerate_POPULATED)
  FetchContent_Populate(samplerate)
  add_subdirectory(${samplerate_SOURCE_DIR} ${samplerate_BINARY_DIR} EXCLUDE_FROM_ALL)
  list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${samplerate_BINARY_DIR}/src)
endif()

list(APPEND FREEDV_LINK_LIBS samplerate)

target_include_directories(samplerate BEFORE PRIVATE ${samplerate_BINARY_DIR})
include_directories(${samplerate_SOURCE_DIR}/include)
