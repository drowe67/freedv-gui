if(WIN32)
message(FATAL_ERROR "mimalloc is only supported on Linux and macOS")
endif(WIN32)

set(MIMALLOC_CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DMI_BUILD_TESTS=0)

if (NOT APPLE)
# Ensures that we're able to compile/run on e.g. Raspberry Pi 4.
# For ARM Macs, the compilation flags injected by mimalloc are fine.
set(MIMALLOC_CMAKE_ARGS ${MIMALLOC_CMAKE_ARGS} -DMI_NO_OPT_ARCH=1)
endif (NOT APPLE)

# Build mimalloc library
include(ExternalProject)
ExternalProject_Add(build_mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        v2.2.4
    UPDATE_DISCONNECTED 1
    CMAKE_ARGS ${MIMALLOC_CMAKE_ARGS}
    CMAKE_CACHE_ARGS -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES} -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_mimalloc BINARY_DIR)
ExternalProject_Get_Property(build_mimalloc SOURCE_DIR)
add_library(mimalloc-obj OBJECT IMPORTED)
add_dependencies(mimalloc-obj build_mimalloc)

set_target_properties(mimalloc-obj PROPERTIES
    IMPORTED_OBJECTS "${BINARY_DIR}/CMakeFiles/mimalloc-obj.dir/src/static.c.o"
)

include_directories(${SOURCE_DIR}/include)
add_definitions(-DUSING_MIMALLOC)
