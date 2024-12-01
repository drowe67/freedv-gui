set(SOXR_VERSION "0.1.3")

set(BUILD_TESTS OFF CACHE BOOL "Enable unit tests for libsoxr")

include(FetchContent)
FetchContent_Declare(
    soxr
    GIT_REPOSITORY https://github.com/chirlu/soxr.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        ${SOXR_VERSION}
)

FetchContent_GetProperties(soxr)
if(NOT soxr_POPULATED)
  FetchContent_Populate(soxr)
  list(APPEND CMAKE_MODULE_PATH "${soxr_SOURCE_DIR}/cmake/Modules")
  add_subdirectory(${soxr_SOURCE_DIR} ${soxr_BINARY_DIR} EXCLUDE_FROM_ALL)
  list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${soxr_BINARY_DIR}/bin)
endif()

list(APPEND FREEDV_LINK_LIBS soxr)

target_include_directories(soxr BEFORE PRIVATE ${soxr_BINARY_DIR})
include_directories(${soxr_SOURCE_DIR}/src)
