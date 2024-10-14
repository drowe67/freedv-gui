set(WXWIDGETS_VERSION "3.2.6")

# Ensure that the wxWidgets library is staticly built.
set(wxBUILD_SHARED OFF CACHE BOOL "Build wx libraries as shared libs")
set(wxBUILD_PRECOMP OFF CACHE BOOL "Use precompiled headers")
set(wxBUILD_MONOLITHIC OFF CACHE BOOL "Build a single library")

# wxWidgets features to enable/disable.
set(wxUSE_STL OFF CACHE STRING "use C++ STL classes")
set(wxUSE_REGEX "builtin" CACHE STRING "enable support for wxRegEx class")
set(wxUSE_ZLIB "builtin" CACHE STRING "Use built-in zlib")
set(wxUSE_EXPAT "builtin" CACHE STRING "Use built-in expat")
set(wxUSE_LIBJPEG "builtin" CACHE STRING "use libjpeg (JPEG file format)")
set(wxUSE_LIBPNG "builtin" CACHE STRING "use libpng (PNG image format)")
set(wxUSE_LIBTIFF "builtin" CACHE STRING "use libtiff (TIFF file format)")
set(wxUSE_NANOSVG OFF CACHE STRING "use NanoSVG for rasterizing SVG")
set(wxUSE_LIBLZMA OFF CACHE STRING "use liblzma for LZMA compression")
set(wxUSE_LIBSDL OFF CACHE STRING "use SDL for audio on Unix")
set(wxUSE_LIBMSPACK OFF CACHE STRING "use libmspack (CHM help files loading)")
set(wxUSE_LIBICONV OFF CACHE STRING "disable use of libiconv")

include(FetchContent)
FetchContent_Declare(
    wxWidgets
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        v${WXWIDGETS_VERSION}
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
