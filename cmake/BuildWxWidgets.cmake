set(WXWIDGETS_VERSION "3.3.1")

# Ensure that the wxWidgets library is staticly built.
set(wxBUILD_SHARED OFF CACHE BOOL "Build wx libraries as shared libs" FORCE)
set(wxBUILD_PRECOMP OFF CACHE BOOL "Use precompiled headers" FORCE)
set(wxBUILD_MONOLITHIC OFF CACHE BOOL "Build a single library" FORCE)

# wxWidgets features to enable/disable.
set(wxUSE_STL OFF CACHE STRING "use C++ STL classes" FORCE)
set(wxUSE_REGEX "builtin" CACHE STRING "enable support for wxRegEx class" FORCE)

if (NOT LINUX)
# Clang will not build the built-in zlib on Linux, so use the system one
# instead on that platform.
set(wxUSE_ZLIB "builtin" CACHE STRING "Use built-in zlib" FORCE)
endif (NOT LINUX)

set(wxUSE_EXPAT "builtin" CACHE STRING "Use built-in expat" FORCE)
set(wxUSE_LIBJPEG "builtin" CACHE STRING "use libjpeg (JPEG file format)" FORCE)
set(wxUSE_LIBPNG "builtin" CACHE STRING "use libpng (PNG image format)" FORCE)
set(wxUSE_LIBTIFF "builtin" CACHE STRING "use libtiff (TIFF file format)" FORCE)
set(wxUSE_NANOSVG OFF CACHE STRING "use NanoSVG for rasterizing SVG" FORCE)
set(wxUSE_LIBLZMA OFF CACHE STRING "use liblzma for LZMA compression" FORCE)
set(wxUSE_LIBSDL OFF CACHE STRING "use SDL for audio on Unix" FORCE)
set(wxUSE_LIBMSPACK OFF CACHE STRING "use libmspack (CHM help files loading)" FORCE)
set(wxUSE_LIBICONV OFF CACHE STRING "disable use of libiconv" FORCE)

set(wxUSE_SOCKETS ON CACHE BOOL "Use sockets" FORCE)
set(wxUSE_IPV6 ON CACHE BOOL "Use IPv6 sockets" FORCE)

if(WIN32)
set(wxUSE_GRAPHICS_DIRECT2D ON CACHE BOOL "use Direct2D graphics context" FORCE)
set(wxUSE_WINSOCK2 ON CACHE BOOL "Use WinSock2" FORCE)
endif(WIN32)

include(FetchContent)
FetchContent_Declare(
    wxWidgets
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        v${WXWIDGETS_VERSION}
    PATCH_COMMAND  git apply ${CMAKE_SOURCE_DIR}/cmake/wxWidgets-Direct2D-color-font.patch
    UPDATE_DISCONNECTED 1
)

FetchContent_GetProperties(wxWidgets)
if(NOT wxwidgets_POPULATED)
  FetchContent_Populate(wxWidgets)
  add_subdirectory(${wxwidgets_SOURCE_DIR} ${wxwidgets_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Override some CXX flags to prevent wxWidgets build failures
if(APPLE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")
endif(APPLE)

# Get required wxWidgets include paths and build definitions. 
# target_link_libraries() will actually do the linking.
get_target_property(WXBUILD_BUILD_DEFS wx::core COMPILE_DEFINITIONS)
get_target_property(WXBUILD_INCLUDES wx::core INTERFACE_INCLUDE_DIRECTORIES)
list(REMOVE_ITEM WXBUILD_BUILD_DEFS "WXBUILDING")
