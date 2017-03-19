#
# FreeDV - HF Digital Voice for Radio Amateurs
#
# CMake configuration contributed by Richard Shaw (KF5OIM)
# Please report questions, comments, problems, or patches to the freetel
# mailing list: https://lists.sourceforge.net/lists/listinfo/freetel-codec2
#

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.7" CACHE STRING "Minimum OS X deployment version")

cmake_minimum_required(VERSION 2.8)

# Prevent in-source builds to protect automake/autoconf config.
# If an in-source build is attempted, you will still need to clean up a few
# files manually.
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds in ${CMAKE_BINARY_DIR} are not "   
   "allowed, please remove ./CMakeCache.txt and ./CMakeFiles/, create a "
   "separate build directory and run cmake from there.")
endif("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")

# Set local module path.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

project(FreeDV)

#
# Set FreeDV version and generate src/version.h
#
set(FREEDV_VERSION_MAJOR 1)
set(FREEDV_VERSION_MINOR 2)
set(FREEDV_VERSION_PATCH FALSE)
set(FREEDV_VERSION ${FREEDV_VERSION_MAJOR}.${FREEDV_VERSION_MINOR})
if(FREEDV_VERSION_PATCH)
    set(FREEDV_VERSION ${FREEDV_VERSION}.${FREEDV_VERSION_PATCH})
endif()
set(FREEDV_VERSION_SUFFIX "devel")
if(FREEDV_VERSION_SUFFIX)
    set(FREEDV_VERSION_STRING "${FREEDV_VERSION} ${FREEDV_VERSION_SUFFIX}")
else()
    set(FREEDV_VERSION_STRING "${FREEDV_VERSION}")
endif()
message(STATUS "FreeDV version: ${FREEDV_VERSION_STRING}")
configure_file(cmake/version.h.in src/version.h @ONLY)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "Build type not specified, defaulting to ${CMAKE_BUILD_TYPE}")
endif(NOT CMAKE_BUILD_TYPE)

# Work around for not using a svn working copy.
add_definitions(-D_NO_AUTOTOOLS_)
find_program(SVN_PATH svn)
if(SVN_PATH)
    execute_process(COMMAND ${SVN_PATH} info --show-item revision
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE SVN_REVISION_RESULT
        OUTPUT_VARIABLE SVN_CURRENT_REVISION
        ERROR_QUIET
    )
else()
    set(SVN_REVISION_RESULT 1)
endif()

if(SVN_REVISION_RESULT EQUAL 0)
    string(STRIP ${SVN_CURRENT_REVISION} SVN_REVISION)
    add_definitions(-DSVN_REVISION="${SVN_REVISION}")
else()
    add_definitions(-DSVN_REVISION="None")
endif()


# Set default build flags.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++11")
endif(APPLE)

#
# Setup cmake options
#
set(CMAKE_VERBOSE_MAKEFILE TRUE CACHE BOOL "Verbose makefile.")
set(USE_STATIC_DEPS FALSE CACHE BOOL 
    "Download and build static libraries instead of system libraries.")
set(USE_STATIC_PORTAUDIO FALSE CACHE BOOL 
    "Download and build static portaudio instead of the system library.")
set(USE_STATIC_SNDFILE FALSE CACHE BOOL
    "Download and build static sndfile instead of the system library.")
set(USE_STATIC_SAMPLERATE FALSE CACHE BOOL
    "Download and build static samplerate instead of the system library.")
set(USE_STATIC_CODEC2 TRUE CACHE BOOL
    "Download and build static codec2 instead of the system library.")
set(USE_STATIC_SPEEXDSP TRUE CACHE BOOL
    "Download and build static speex instead of the system library.")
set(BOOTSTRAP_WXWIDGETS FALSE CACHE BOOL
    "Download and build static wxWidgets instead of the system library.")

if(USE_STATIC_DEPS)
    set(USE_STATIC_PORTAUDIO TRUE FORCE)
    set(USE_STATIC_SNDFILE TRUE FORCE)
    set(USE_STATIC_SAMPLERATE TRUE FORCE)
    set(USE_STATIC_CODEC2 TRUE FORCE)
endif(USE_STATIC_DEPS)

#
# Pull in external wxWidgets target if performing static build.
#
if(BOOTSTRAP_WXWIDGETS)
    message(STATUS "Adding wxWidgets build target...")
    include(cmake/BuildWxWidgets.cmake)
endif(BOOTSTRAP_WXWIDGETS)

#
# Perform bootstrap build of wxWidgets
#
if(BOOTSTRAP_WXWIDGETS AND NOT EXISTS ${WXCONFIG})
    message(STATUS "Will perform bootstrap build of wxWidgets.
   After make step completes, re-run cmake and make again to perform FreeDV build.")
#
# Continue normal build if not bootstrapping wxWidgets or is already built.
#
else(BOOTSTRAP_WXWIDGETS AND NOT EXISTS ${WXCONFIG})


#
# Various hacks and work arounds for building under MinGW.
#
if(MINGW)
    message(STATUS "System is MinGW.")
    # Setup HOST variable.
    include(cmake/MinGW.cmake)
    # This sets up the exe icon for windows under mingw.
    set(RES_FILES "")
    set(RES_FILES "${CMAKE_SOURCE_DIR}/contrib/freedv.rc")
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
    include(InstallRequiredSystemLibraries)
endif(MINGW)

# Math library is automatic on MinGW
if(UNIX)
    set(CMAKE_REQUIRED_INCLUDES math.h)
    set(CMAKE_REQUIRED_LIBRARIES m)
endif(UNIX)

# Find some standard headers and functions.
include(CheckIncludeFiles)
check_include_files("byteswap.h"   HAVE_BYTESWAP_H)
check_include_files("limits.h"     HAVE_LIMITS_H)
check_include_files("stddef.h"     HAVE_STDDEF_H)
check_include_files("stdlib.h"     HAVE_STDLIB_H)
check_include_files("string.h"     HAVE_STRING_H)
check_include_files("strings.h"    HAVE_STRINGS_H)
check_include_files("ltdl.h"       HAVE_LTDL_H)
check_include_files("inttypes.h"   HAVE_INTTYPES_H)
check_include_files("sys/stat.h"   HAVE_SYS_STAT_H)
check_include_files("sys/types.h"  HAVE_SYS_TYPES_H)

include(CheckTypeSize)
check_type_size("int" SIZEOF_INT)

include(CheckFunctionExists)
check_function_exists(floor HAVE_FLOOR)
check_function_exists(memset HAVE_MEMSET)
check_function_exists(pow HAVE_POW)
check_function_exists(sqrt HAVE_SQRT)
check_function_exists(fseeko HAVE_FSEEKO)
check_function_exists(fmemopen HAVE_FMEMOPEN)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(vsnprintf HAVE_VSNPRINTF)

include(CheckSymbolExists)
check_symbol_exists("_fseeki64" "stdio.h" HAVE__FSEEKI64)

# fdmdv2_main.h requires patching to find config.h as it current looks in the
# source directory and the generated file goes in the binary directory.
configure_file ("${PROJECT_SOURCE_DIR}/cmake/config.h.in"
                "${PROJECT_BINARY_DIR}/config.h" )
include_directories(${PROJECT_BINARY_DIR})
add_definitions(-DHAVE_CONFIG_H)

# Config file for bundled sox sources
configure_file("${PROJECT_SOURCE_DIR}/cmake/soxconfig.h.in"
               "${PROJECT_BINARY_DIR}/soxconfig.h")

# Pthread Library
find_package(Threads REQUIRED)
message(STATUS "Threads library flags: ${CMAKE_THREAD_LIBS_INIT}")

#
# Find codec2
#
if(NOT USE_STATIC_CODEC2)
    message(STATUS "Looking for codec2...")
    # 'CONFIG' removed due to incompatibility with cmake version
    # in Ubuntu 12.04 (Precise) -- Stuart Longland
    find_package(codec2 QUIET)
    if(codec2_FOUND)
        get_target_property(CODEC2_LIBRARY codec2 LOCATION)
        message(STATUS "  codec2 library: ${CODEC2_LIBRARY}")
        message(STATUS "  codec2 headers: ${codec2_INCLUDE_DIRS}")
    else()
        # Try to find manually
        find_path(CODEC2_INCLUDE_DIRS codec2.h
                  PATH_SUFFIXES codec2)
        find_library(CODEC2_LIBRARY NAMES codec2)
        if(CODEC2_LIBRARY AND CODEC2_INCLUDE_DIRS)
            message(STATUS "  codec2 library: ${CODEC2_LIBRARY}")
            message(STATUS "  codec2 headers: ${CODEC2_INCLUDE_DIRS}")
            list(APPEND FREEDV_LINK_LIBS ${CODEC2_LIBRARY})
            include_directories(${CODEC2_INCLUDE_DIRS})
        else()
            message(FATAL_ERROR "codec2 library not found.
Linux: 
Codec2 may not be in your distribution so build yourself or use the cmake option to build statically into FreeDV.
Windws:
It's easiest to use the cmake option: USE_STATIC_CODEC2"
        )
        endif()
    endif()
else(NOT USE_STATIC_CODEC2)
    message(STATUS "Will attempt static build of codec2.")
    include(cmake/BuildCodec2.cmake)
endif(NOT USE_STATIC_CODEC2)

#
# Find or build portaudio Library
#
if(NOT USE_STATIC_PORTAUDIO)
    message(STATUS "Looking for portaudio...")
    find_package(Portaudio REQUIRED)
    if(PORTAUDIO_FOUND)
        message(STATUS "  portaudio library: ${PORTAUDIO_LIBRARIES}")
        message(STATUS "  portaudio headers: ${PORTAUDIO_INCLUDE_DIRS}")
        list(APPEND FREEDV_LINK_LIBS ${PORTAUDIO_LIBRARIES})
        include_directories(${PORTAUDIO_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "portaudio library not found.
On Linux systems try installing:
    portaudio-devel  (RPM based systems)
    libportaudio-dev (DEB based systems)
On Windows it's easiest to use the cmake option: USE_STATIC_PORTAUDIO"
        )
    endif()
    if(NOT ${PORTAUDIO_VERSION} EQUAL 19)
        message(WARNING "Portaudio versions other than 19 are known to have issues. You have been warned!")
    endif()
else(NOT USE_STATIC_PORTAUDIO)
    message(STATUS "Will attempt static build of portaudio.")
    include(cmake/BuildPortaudio.cmake)
endif(NOT USE_STATIC_PORTAUDIO)

#
# Hamlib library
#
message(STATUS "Looking for hamlib...")
find_path(HAMLIB_INCLUDE_DIR hamlib/rig.h)
find_library(HAMLIB_LIBRARY hamlib PATH_SUFFIXES hamlib)
message(STATUS "Hamlib library: ${HAMLIB_LIBRARY}")
message(STATUS "Hamlib headers: ${HAMLIB_INCLUDE_DIR}")
if(HAMLIB_LIBRARY AND HAMLIB_INCLUDE_DIR)
    message(STATUS "Hamlib library found.")
    include_directories(${HAMLIB_INCLUDE_DIR})
    list(APPEND FREEDV_LINK_LIBS ${HAMLIB_LIBRARY})
else(HAMLIB_LIBRARY AND HAMLIB_INCLUDE_DIR)
    message(FATAL_ERROR "hamlib not found.
On Linux systems try installing:
    hamlib-devel  (RPM based systems)
    libhamlib-dev (DEB based systems)"
    )
endif(HAMLIB_LIBRARY AND HAMLIB_INCLUDE_DIR)


#
# Samplerate Library
#
if(NOT USE_STATIC_SAMPLERATE)
    message(STATUS "Looking for samplerate...")
    find_library(LIBSAMPLERATE samplerate)
    find_path(LIBSAMPLERATE_INCLUDE_DIR samplerate.h)
    message(STATUS "  samplerate library: ${LIBSAMPLERATE}")
    message(STATUS "  samplerate headers: ${LIBSAMPLERATE_INCLUDE_DIR}")
    if(LIBSAMPLERATE AND LIBSAMPLERATE_INCLUDE_DIR)
        list(APPEND FREEDV_LINK_LIBS ${LIBSAMPLERATE})
        include_directories(${LIBSAMPLERATE_INCLUDE_DIR})
    else(LIBSTAMPLERATE AND LIBSAMPLERATE_INCLUDE_DIR)
        message(FATAL_ERROR "samplerate library not found.
On Linux systems try installing:
    samplerate-devel  (RPM based systems)
    libsamplerate-dev (DEB based systems)
On Windows it's easiest to use the cmake option: USE_STATIC_SAMPLERATE"
        )
    endif(LIBSAMPLERATE AND LIBSAMPLERATE_INCLUDE_DIR)
else(NOT USE_STATIC_SAMPLERATE)
    message(STATUS "Will attempt static build of samplerate.")
    include(cmake/BuildSamplerate.cmake)
endif(NOT USE_STATIC_SAMPLERATE)

#    
# sndfile Library
#
if(NOT USE_STATIC_SNDFILE)
    message(STATUS "Looking for sndfile...")
    find_library(LIBSNDFILE sndfile)
    find_path(LIBSNDFILE_INCLUDE_DIR sndfile.h)
    message(STATUS "  sndfile library: ${LIBSNDFILE}")
    message(STATUS "  sndfile headers: ${LIBSNDFILE_INCLUDE_DIR}")
    if(LIBSNDFILE AND LIBSNDFILE_INCLUDE_DIR)
        list(APPEND FREEDV_LINK_LIBS ${LIBSNDFILE})
    else(LIBSNDFILE AND LIBSNDFILE_INCLUDE_DIR)
        message(FATAL_ERROR "sndfile library not found.
On Linux systems try installing:
    libsndfile-devel (RPM based systems)
    libsndfile-dev   (DEB based systems)
On Windows it's easiest to use the cmake option: USE_STATIC_SNDFILE"
        )
    endif(LIBSNDFILE AND LIBSNDFILE_INCLUDE_DIR)
else(NOT USE_STATIC_SNDFILE)
    message(STATUS "Will attempt static build of sndfile.")
    include(cmake/BuildSndfile.cmake)
endif(NOT USE_STATIC_SNDFILE)

#
# Find wxWidgets
#
if(NOT BOOTSTRAP_WXWIDGETS)
    set(WXCONFIG "" CACHE FILEPATH "Location of wx-config binary.")
    set(WXRC "" CACHE FILEPATH "Location of wxrc binary.")
endif(NOT BOOTSTRAP_WXWIDGETS)
#if(BOOTSTRAP_WXWIDGETS)
#    set(WXCONFIG "${CMAKE_BINARY_DIR}/external/dist/bin/wx-config")
#    set(WXRC "${CMAKE_BINARY_DIR}/external/dist/bin/wxrc")
#    list(APPEND FREEDV_STATIC_DEPS wxWidgets)
#endif(BOOTSTRAP_WXWIDGETS)
message(STATUS "Looking for wxWidgets...")
if(WXCONFIG)
    message(STATUS "wx-config: ${WXCONFIG}")
    set(wxWidgets_CONFIG_EXECUTABLE ${WXCONFIG})
endif(WXCONFIG)
if(WXRC)
    message(STATUS "wxrc: ${WXRC}")
    set(wxWidgets_wxrc_EXECUTABLE ${WXRC})
endif(WXRC)
set(WX_VERSION_MIN 3.0.0)
find_package(wxWidgets REQUIRED core base aui html net adv)
execute_process(COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}" --version
    OUTPUT_VARIABLE WX_VERSION)
string(STRIP ${WX_VERSION} WX_VERSION)
if(WX_VERSION VERSION_EQUAL ${WX_VERSION_MIN}
    OR WX_VERSION VERSION_GREATER ${WX_VERSION_MIN})
   message(STATUS "wxWidgets version: ${WX_VERSION}")
else()
    message(FATAL_ERROR "wxWidgets must be installed on your system.
Please check that wx-config is in path, the directory
where wxWidgets libraries are installed (returned by
'wx-config --libs' or 'wx-config --static --libs' command)
is in LD_LIBRARY_PATH or equivalent variable and
wxWidgets version is ${WX_VERSION_MIN} or above.")
endif()
if(wxWidgets_FOUND)
    include("${wxWidgets_USE_FILE}")
    list(APPEND FREEDV_LINK_LIBS ${wxWidgets_LIBRARIES})
endif(wxWidgets_FOUND)

#
# Find speex library
#
if(NOT USE_STATIC_SPEEXDSP)
    message(STATUS "Looking for Speex DSP library.")
    find_path(SPEEXDSP_INCLUDE_DIR NAMES speex/speex.h speex/speexdsp_types.h)
    find_library(SPEEXDSP_LIBRARY speexdsp)
    message(STATUS "  Speex DSP headers: ${SPEEXDSP_INCLUDE_DIR}")
    message(STATUS "  Speex DSP library: ${SPEEXDSP_LIBRARY}")
    if(SPEEXDSP_INCLUDE_DIR AND SPEEXDSP_LIBRARY)
        include_directories(${SPEEXDSP_INCLUDE_DIR})
        list(APPEND FREEDV_LINK_LIBS ${SPEEXDSP_LIBRARY})
    else(SPEEXDSP_INCLUDE_DIR AND SPEEXDSP_LIBRARY)
        message(FATAL_ERROR "Speex DSP library not found!")
    endif(SPEEXDSP_INCLUDE_DIR AND SPEEXDSP_LIBRARY)
else()
    message(STATUS "Will attempt static build of speex.")
    include(cmake/BuildSpeex.cmake)
endif()

#
# Find libdl for dlopen/dlclose
#
if(UNIX)
    message(STATUS "Looking for dl library.")
    find_library(DL_LIBRARY dl)
    if(DL_LIBRARY)
        message(STATUS "  dl library: ${DL_LIBRARY}")
        list(APPEND FREEDV_LINK_LIBS ${DL_LIBRARY})
    else()
        message(FATAL_ERROR "dl library not found.
On Linux systems try installing:
    glibc-devel  (RPM based systems)
    glibc-dev (DEB based systems)"
        )
    endif()
endif(UNIX)

    
#Freedv 
add_subdirectory(src)

# Icons and desktop file
add_subdirectory(contrib)

message(STATUS "Build type will be: ${CMAKE_BUILD_TYPE}")

#
# Cpack NSIS configuration for Windows.
#
if(WIN32)
    # Detect if we're doing a 32-bit or 64-bit windows build.
    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(CMAKE_CL_64 TRUE)
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    endif()
    if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        set(CPACK_STRIP_FILES TRUE)
    endif()
    configure_file(cmake/GetDependencies.cmake.in cmake/GetDependencies.cmake
        @ONLY
    )
    install(SCRIPT ${CMAKE_BINARY_DIR}/cmake/GetDependencies.cmake)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HF Digital Voice for Radio Amateurs")
    set(CPACK_PACKAGE_VENDOR "CMake")
    #set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_PACKAGE_VERSION_MAJOR ${FREEDV_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${FREEDV_VERSION_MINOR})
    # CPack expects a patch level version so set it here and override if we
    # are actually setting one.
    set(CPACK_PACKAGE_VERSION_PATCH 0)
    if(FREEDV_VERSION_PATCH)
        set(CPACK_PACKAGE_VERSION_PATCH ${FREEDV_VERSION_PATCH})
    endif()
    if(FREEDV_VERSION_SUFFIX)
        set(CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH}-${FREEDV_VERSION_SUFFIX}")
    endif()
    # There is a bug in NSI that does not handle full unix paths properly. Make
    # sure there is at least one set of four (4) backlasshes.
    #set(CPACK_PACKAGE_ICON "${CMake_SOURCE_DIR}/Utilities/Release\\\\InstallIcon.bmp")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\freedv.exe")
    set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY}")
    set(CPACK_NSIS_PACKAGE_NAME "FreeDV")
    set(CPACK_PACKAGE_EXECUTABLES freedv;FreeDV)
    set(CPACK_NSIS_URL_INFO_ABOUT "http://freedv.org")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_MENU_LINKS
        "http://freedv.org" "FreeDV Homepage")
    include(CPack)
endif(WIN32)

endif(BOOTSTRAP_WXWIDGETS AND NOT EXISTS ${WXCONFIG})
