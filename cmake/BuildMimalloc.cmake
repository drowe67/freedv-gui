if(WIN32)
message(FATAL_ERROR "mimalloc is only supported on Linux and macOS")
endif(WIN32)

set(MI_BUILD_TESTS FALSE)

if (NOT APPLE)
# Ensures that we're able to compile/run on e.g. Raspberry Pi 4.
# For ARM Macs, the compilation flags injected by mimalloc are fine.
set(MI_NO_OPT_ARCH TRUE)
endif (NOT APPLE)

include(FetchContent)
FetchContent_Declare(
    mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        v2.2.4
    UPDATE_DISCONNECTED 1
)

FetchContent_GetProperties(mimalloc)
if(NOT mimalloc_POPULATED)
  FetchContent_Populate(mimalloc)
  add_subdirectory(${mimalloc_SOURCE_DIR} ${mimalloc_BINARY_DIR})
  include_directories(${mimalloc_SOURCE_DIR}/include)
  add_definitions(-DUSING_MIMALLOC)
endif()
