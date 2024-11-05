#
# FreeDV - HF Digital Voice for Radio Amateurs
#
# CMake configuration contributed by Richard Shaw (KF5OIM)
# Please report questions, comments, problems, or patches to the freetel
# mailing list: https://lists.sourceforge.net/lists/listinfo/freetel-codec2
#
cmake_minimum_required(VERSION 3.13)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "Minimum OS X deployment version")

set(PROJECT_NAME FreeDV)
set(PROJECT_VERSION 2.0.0)
set(PROJECT_DESCRIPTION "HF Digital Voice for Radio Amateurs")
set(PROJECT_HOMEPAGE_URL "https://freedv.org")

# Makes FreeDV overridden CMake platform available for ARM MinGW builds.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

if(APPLE)
    project(
        ${PROJECT_NAME}
        VERSION ${PROJECT_VERSION}
        DESCRIPTION ${PROJECT_DESCRIPTION}
        HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL}
        LANGUAGES C CXX OBJCXX
    )

    # Workaround for Xcode 15 bug preventing FreeDV binaries from starting
    # on older versions of macOS. See https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking.
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0.0.0)
        add_link_options("-Wl,-ld_classic")
    endif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0.0.0)
else(APPLE)
    project(
        ${PROJECT_NAME}
        VERSION ${PROJECT_VERSION}
        DESCRIPTION ${PROJECT_DESCRIPTION}
        HOMEPAGE_URL ${PROJECT_HOMEPAGE_URL}
        LANGUAGES C CXX
    )
endif(APPLE)    

# Adds a tag to the end of the version string. Leave empty
# for official release builds.
if(NOT DEFINED FREEDV_VERSION_TAG)
set(FREEDV_VERSION_TAG "devel")
add_definitions(-DUNOFFICIAL_RELEASE)
endif(NOT DEFINED FREEDV_VERSION_TAG)

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

# Build universal ARM64 and x86_64 binaries on Mac.
if(BUILD_OSX_UNIVERSAL)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif(BUILD_OSX_UNIVERSAL)

# Return the date (yyyy-mm-dd)
string(TIMESTAMP DATE_RESULT "%Y-%m-%d" UTC)
message(STATUS "Compilation date = XX${DATE_RESULT}XX")
add_definitions(-DFREEDV_BUILD_DATE="${DATE_RESULT}")

#
# generate src/version.h
#
configure_file(cmake/version.h.in src/version.h @ONLY)

# Generate custom CTest config
configure_file(cmake/CTestCustom.cmake ${CMAKE_BINARY_DIR})

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "Build type not specified, defaulting to ${CMAKE_BUILD_TYPE}")
endif(NOT CMAKE_BUILD_TYPE)

# Work around for not using a svn working copy.
#add_definitions(-D_NO_AUTOTOOLS_)

#
# Find the git hash if this is a working copy.
#
if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --always HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE res
            OUTPUT_VARIABLE FREEDV_HASH
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "freedv-gui current git hash: ${FREEDV_HASH}")
        add_definitions(-DGIT_HASH="${FREEDV_HASH}")
    else()
        message(WARNING "Git not found. Can not determine current commit hash.")
        add_definitions(-DGIT_HASH="Unknown")
    endif()
else()
        add_definitions(-DGIT_HASH="None")
endif()

# Set default build flags.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif(APPLE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# Setup cmake options
#
set(CMAKE_VERBOSE_MAKEFILE TRUE CACHE BOOL "Verbose makefile.")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE CACHE BOOL "Enable IDE features.")
set(USE_INTERNAL_CODEC2 FALSE CACHE BOOL
    "Download and build codec2  instead of using the system library.")
set(BOOTSTRAP_LPCNET FALSE CACHE BOOL
    "Download and build LPCNet instead of using the system library.")
set(USE_STATIC_DEPS FALSE CACHE BOOL
    "Download and build static libraries instead of system libraries.")
set(USE_STATIC_PORTAUDIO FALSE CACHE BOOL
    "Download and build static portaudio instead of the system library.")
set(USE_STATIC_SNDFILE FALSE CACHE BOOL
    "Download and build static sndfile instead of the system library.")
set(USE_STATIC_SAMPLERATE FALSE CACHE BOOL
    "Download and build static samplerate instead of the system library.")
set(USE_STATIC_SPEEXDSP FALSE CACHE BOOL
    "Download and build static speex instead of the system library.")
set(BOOTSTRAP_WXWIDGETS FALSE CACHE BOOL
    "Download and build static wxWidgets instead of the system library.")
set(USE_PULSEAUDIO TRUE CACHE BOOL
    "Use PulseAudio instead of PortAudio for audio I/O.")
set(SIGN_WINDOWS_BINARIES FALSE CACHE BOOL
    "Enable signing of Windows binaries. See CODE_SIGNING.md for info.")

if(SIGN_WINDOWS_BINARIES)
    if(NOT WIN32 AND NOT MINGW)
        message(FATAL_ERROR "Signing only makes sense for Windows builds.")
    endif(NOT WIN32 AND NOT MINGW)

    if(NOT DEFINED PKCS11_KEY_FILE OR NOT DEFINED PKCS11_CERTIFICATE_FILE OR NOT DEFINED INTERMEDIATE_CERT_FILE)
        message(FATAL_ERROR "Must provide key and certifcate files.")
    endif(NOT DEFINED PKCS11_KEY_FILE OR NOT DEFINED PKCS11_CERTIFICATE_FILE OR NOT DEFINED INTERMEDIATE_CERT_FILE)

    # Default PKCS#11 engine if not provided.
    if(NOT DEFINED PKCS11_ENGINE)
        # XXX -- assuming Ubuntu on amd64
        set(PKCS11_ENGINE /usr/lib/x86_64-linux-gnu/engines-3/pkcs11.so)
    endif(NOT DEFINED PKCS11_ENGINE)
  
    # Default PKCS#11 module if not provided.
    if(NOT DEFINED PKCS11_MODULE)
        # XXX -- we're assuming Ubuntu amd64 and SafeNet here.
        set(PKCS11_MODULE /usr/lib/libeToken.so)
    endif(NOT DEFINED PKCS11_MODULE)

    # Default timestamping server
    if(NOT DEFINED TIMESTAMP_SERVER)
        set(TIMESTAMP_SERVER http://timestamp.digicert.com/)    
    endif(NOT DEFINED TIMESTAMP_SERVER)

    # Default hash algorithm
    if(NOT DEFINED SIGN_HASH)
        set(SIGN_HASH sha256)
    endif(NOT DEFINED SIGN_HASH)

    # Look for osslsigncode    
    find_program(
        OSSLSIGNCODE_BIN
        osslsigncode
        REQUIRED)

    # Generate command to execute for signing binaries.
    file(READ ${PKCS11_KEY_FILE} PKCS11_KEY_TMP)
    file(READ ${PKCS11_CERTIFICATE_FILE} PKCS11_CERTIFICATE_TMP)
    string(REPLACE "\n" "" PKCS11_KEY "${PKCS11_KEY_TMP}")
    string(REPLACE "\n" "" PKCS11_CERTIFICATE "${PKCS11_CERTIFICATE_TMP}")
endif(SIGN_WINDOWS_BINARIES)

if(USE_STATIC_DEPS)
    set(USE_STATIC_PORTAUDIO TRUE FORCE)
    set(USE_STATIC_SNDFILE TRUE FORCE)
    set(USE_STATIC_SAMPLERATE TRUE FORCE)
    set(USE_STATIC_SPEEXDSP TRUE FORCE)
endif(USE_STATIC_DEPS)

#
# Find wxWidgets
#
set(DARK_MODE_DISABLE "false")
if(NOT BOOTSTRAP_WXWIDGETS)
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
    set(WX_31_VERSION_MIN 3.1.3)
    find_package(wxWidgets COMPONENTS core base aui html net adv propgrid)
    if(NOT wxWidgets_FOUND)
        set(BOOTSTRAP_WXWIDGETS TRUE)
    else()
        execute_process(COMMAND sh "${wxWidgets_CONFIG_EXECUTABLE}" --version
            OUTPUT_VARIABLE WX_VERSION)
        string(STRIP ${WX_VERSION} WX_VERSION)
        if(WX_VERSION VERSION_EQUAL ${WX_VERSION_MIN}
           OR WX_VERSION VERSION_GREATER ${WX_VERSION_MIN})
            message(STATUS "wxWidgets version: ${WX_VERSION}")
        else()
            set(BOOTSTRAP_WXWIDGETS TRUE)
        endif()
    endif()
endif()

if(BOOTSTRAP_WXWIDGETS)
    message(STATUS "Adding wxWidgets build target...")
    include(cmake/BuildWxWidgets.cmake)
else()
    if(APPLE)
        if(WX_VERSION VERSION_EQUAL ${WX_31_VERSION_MIN}
           OR WX_VERSION VERSION_GREATER ${WX_31_VERSION_MIN})
            set(DARK_MODE_DISABLE "false")
        else()
            set(DARK_MODE_DISABLE "true")
       endif()
    endif(APPLE)
endif()

#
# Various hacks and work arounds for building under MinGW.
#
if(MINGW)
    message(STATUS "System is MinGW.")
    # Default to static build of portaudio until I can figure out why the
    # shared DLL doesn't work.
    set(USE_STATIC_PORTAUDIO TRUE)
    unset(USE_INTERNAL_CODEC2 CACHE)
    set(USE_INTERNAL_CODEC2 TRUE CACHE BOOL "Perform internal builds of codec2")
    # Setup HOST variables.
    include(cmake/MinGW.cmake)
    # This sets up the exe icon for windows under mingw.
    set(RES_FILES "")

    set(RES_FILES "${CMAKE_BINARY_DIR}/freedv.rc")
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

# main.h requires patching to find config.h as it current looks in the
# source directory and the generated file goes in the binary directory.
configure_file ("${PROJECT_SOURCE_DIR}/cmake/config.h.in"
                "${PROJECT_BINARY_DIR}/config.h" )
include_directories(${PROJECT_BINARY_DIR})

# Config file for bundled sox sources
configure_file("${PROJECT_SOURCE_DIR}/cmake/soxconfig.h.in"
               "${PROJECT_BINARY_DIR}/soxconfig.h")

# Pthread Library
# Actually not needed if compiling with c++11 or higher...
find_package(Threads REQUIRED)
message(STATUS "Threads library flags: ${CMAKE_THREAD_LIBS_INIT}")

#
# Build RADE. 
# (TBD - allow override with own build, similar to Codec2)
include(cmake/BuildOpus.cmake)
include(cmake/BuildRADE.cmake)

#
# Find codec2 and lpcnetfreedv
#
if(CODEC2_BUILD_DIR)
    if(USE_INTERNAL_CODEC2)
        set(USE_INTERNAL_CODEC2 FALSE)
        message(WARNING "Use of build directories and internal builds are mutually exclusive. Forcing internal builds off.")
    endif()
    set(USE_INTERNAL_CODEC2 OFF CACHE BOOL "" FORCE)
    message(STATUS "Using codec2 from build directory...")

    # Theoretically this isn't needed due to the PATHS argument below, but 
    # Fedora 37 (inside the Docker container) can't find Codec2 without it.
    set(codec2_DIR ${CODEC2_BUILD_DIR})

    find_package(codec2 REQUIRED
        NO_DEFAULT_PATH
        PATHS ${CODEC2_BUILD_DIR}
        CONFIGS codec2.cmake
    )
    get_target_property(CODEC2_INCLUDE_DIRS codec2 INTERFACE_INCLUDE_DIRECTORIES)
    if(LPCNET_BUILD_DIR)
        message(STATUS "Using LPCNet from build directory...")
        set(lpcnet_DIR ${LPCNET_BUILD_DIR})
        find_package(lpcnet REQUIRED
            NO_DEFAULT_PATH
            PATHS ${LPCNET_BUILD_DIR}
            CONFIGS lpcnetfreedv.cmake
        )
    else()
        set(LPCNET_DISABLED YES)
        add_definitions(-DLPCNET_DISABLED)
    endif()
elseif(USE_INTERNAL_CODEC2)
    message(STATUS "Will attempt internal build of codec2 and lpcnetfreedv.")
    include(cmake/BuildCodec2.cmake)
else(CODEC2_BUILD_DIR)
    message(STATUS "Looking for codec2...")
    # 'CONFIG' removed due to incompatibility with cmake version
    # in Ubuntu 12.04 (Precise) -- Stuart Longland
    find_package(codec2 QUIET)
    if(codec2_FOUND)
        get_target_property(CODEC2_LIBRARY codec2 LOCATION)
        get_target_property(CODEC2_INCLUDE_DIRS codec2 INTERFACE_INCLUDE_DIRECTORIES)
        message(STATUS "  codec2 library: ${CODEC2_LIBRARY}")
        message(STATUS "  codec2 headers: ${CODEC2_INCLUDE_DIRS}")
        find_package(lpcnetfreedv REQUIRED)
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
            message(STATUS "Will attempt internal build of codec2 and lpcnetfreedv.")
            include(cmake/BuildCodec2.cmake)
        endif()
    endif()
endif(CODEC2_BUILD_DIR)

if(NOT DEFINED LINUX)
    string(FIND ${CMAKE_SYSTEM_NAME} "Linux" LINUX_LOC)
    if((${LINUX_LOC} GREATER 0) OR (${LINUX_LOC} EQUAL 0))
        set(LINUX ON)
    else((${LINUX_LOC} GREATER 0) OR (${LINUX_LOC} EQUAL 0))
        set(LINUX OFF)
    endif((${LINUX_LOC} GREATER 0) OR (${LINUX_LOC} EQUAL 0))
endif(NOT DEFINED LINUX)

if (NOT USE_PULSEAUDIO AND LINUX)
    message(WARNING "Use of PortAudio on Linux has been deprecated and may be explicitly disallowed in a future release. It is highly recommended that you use FreeDV's built-in PulseAudio support instead as this generally gives better results, especially on distributions that now use pipewire for audio.")
endif(NOT USE_PULSEAUDIO AND LINUX)

#
# Find or build portaudio/PulseAudio Library
#
if(NOT USE_STATIC_PORTAUDIO AND NOT USE_PULSEAUDIO)
    message(STATUS "Looking for portaudio...")
    find_package(portaudio-2.0 REQUIRED)
    if(PORTAUDIO_FOUND)
        message(STATUS "  portaudio library: ${PORTAUDIO_LIBRARIES}")
        message(STATUS "  portaudio headers: ${PORTAUDIO_INCLUDE_DIRS}")
        if(pkgcfg_lib_PORTAUDIO_portaudio)
        list(APPEND FREEDV_LINK_LIBS ${pkgcfg_lib_PORTAUDIO_portaudio})
        else(pkgcfg_lib_PORTAUDIO_portaudio)
        list(APPEND FREEDV_LINK_LIBS ${PORTAUDIO_LIBRARIES})
        endif(pkgcfg_lib_PORTAUDIO_portaudio)
        include_directories(${PORTAUDIO_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "portaudio library not found.
On Linux systems try installing:
    portaudio-devel  (RPM based systems)
    portaudio19-dev (DEB based systems)
On Windows it's easiest to use the cmake option: USE_STATIC_PORTAUDIO"
        )
    endif()
    if(NOT ${PORTAUDIO_VERSION} EQUAL 19 AND NOT MINGW)
        message(WARNING "Portaudio versions other than 19 are known to have issues. You have been warned!")
    endif()
elseif(USE_PULSEAUDIO AND LINUX)
    message(STATUS "Finding PulseAudio...")
    find_package(PulseAudio REQUIRED)
    if(PULSEAUDIO_FOUND)
        message(STATUS "  PulseAudio library: ${PULSEAUDIO_LIBRARY}")
        message(STATUS "  PulseAudio headers: ${PULSEAUDIO_INCLUDE_DIR}")
        list(APPEND FREEDV_LINK_LIBS ${PULSEAUDIO_LIBRARY})
        include_directories(${PULSEAUDIO_INCLUDE_DIR})
        add_definitions(-DAUDIO_ENGINE_PULSEAUDIO_ENABLE)
    else()
        message(FATAL_ERROR "PulseAudio library not found.
On Linux systems try installing:
    pulseaudio-libs-devel  (RPM based systems)
    libpulse-dev           (DEB based systems)
")
     endif()
else()
    message(STATUS "Will attempt static build of portaudio.")
    include(cmake/Buildportaudio-2.0.cmake)
endif()

#
# Hamlib library
#
message(STATUS "Looking for hamlib...")
find_path(HAMLIB_INCLUDE_DIR hamlib/rig.h)
find_library(HAMLIB_LIBRARY hamlib PATH_SUFFIXES hamlib)
message(STATUS "  Hamlib library: ${HAMLIB_LIBRARY}")
message(STATUS "  Hamlib headers: ${HAMLIB_INCLUDE_DIR}")
if(HAMLIB_LIBRARY AND HAMLIB_INCLUDE_DIR)
    message(STATUS "Hamlib library found.")
    include_directories(${HAMLIB_INCLUDE_DIR})
    list(APPEND FREEDV_LINK_LIBS ${HAMLIB_LIBRARY})
else(HAMLIB_LIBRARY AND HAMLIB_INCLUDE_DIR)
    message(STATUS "Using own Hamlib build")
    include(cmake/BuildHamlib.cmake)
endif(HAMLIB_LIBRARY AND HAMLIB_INCLUDE_DIR)


# sioclient library
message(STATUS "Looking for sioclient...")
find_path(SIOCLIENT_INCLUDE_DIR sio_client.h)
find_library(SIOCLIENT_LIBRARY sioclient)
message(STATUS "  Socket.io library: ${SIOCLIENT_LIBRARY}")
message(STATUS "  Socket.io headers: ${SIOCLIENT_INCLUDE_DIR}")
if(SIOCLIENT_LIBRARY AND SIOCLIENT_INCLUDE_DIR)
    message(STATUS "Socket.io library found.")
    include_directories(${SIOCLIENT_INCLUDE_DIR})
    list(APPEND FREEDV_LINK_LIBS ${SIOCLIENT_LIBRARY})
else(SIOCLIENT_LIBRARY AND SIOCLIENT_INCLUDE_DIR)
    message(STATUS "Using static sioclient build")
    include(cmake/BuildSocketIo.cmake)
endif(SIOCLIENT_LIBRARY AND SIOCLIENT_INCLUDE_DIR)


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
        message(STATUS "Will attempt static build of samplerate.")
        include(cmake/BuildSamplerate.cmake)
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
        message(STATUS "Will attempt static build of sndfile.")
        include(cmake/BuildSndfile.cmake)
    endif(LIBSNDFILE AND LIBSNDFILE_INCLUDE_DIR)
else(NOT USE_STATIC_SNDFILE)
    message(STATUS "Will attempt static build of sndfile.")
    include(cmake/BuildSndfile.cmake)
endif(NOT USE_STATIC_SNDFILE)

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
    elseif(MINGW)
        message(STATUS "Could not find speexdsp, reverting to static build.")
        include(cmake/BuildSpeex.cmake)
    else(SPEEXDSP_INCLUDE_DIR AND SPEEXDSP_LIBRARY)
        message(STATUS "Will attempt static build of speex.")
        include(cmake/BuildSpeex.cmake)
    endif(SPEEXDSP_INCLUDE_DIR AND SPEEXDSP_LIBRARY)
else()
    message(STATUS "Will attempt static build of speex.")
    include(cmake/BuildSpeex.cmake)
endif()

#
# Find libdl for dlopen/dlclose
#
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    message("System is Linux")
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
endif()

# Enable CTest tests for freedv-gui
option(UNITTEST "Build unittest binaries." OFF)
if(UNITTEST)
enable_testing()
endif(UNITTEST)

#
# Freedv
#
add_subdirectory(src)

# Icons and desktop file
add_subdirectory(contrib)

message(STATUS "Build type will be: ${CMAKE_BUILD_TYPE}")

# CPack configuration for Windows and Linux.
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HF Digital Voice for Radio Amateurs")
set(CPACK_PACKAGE_VENDOR "CMake")
#set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR ${FreeDV_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${FreeDV_VERSION_MINOR})
# CPack expects a patch level version so set it here and override if we
# are actually setting one.
set(CPACK_PACKAGE_VERSION_PATCH 0)
if(FreeDV_VERSION_PATCH)
    set(CPACK_PACKAGE_VERSION_PATCH ${FreeDV_VERSION_PATCH})
endif()
if(FreeDV_VERSION_TWEAK)
    set(CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH}.${FreeDV_VERSION_TWEAK}")
    set(CPACK_VER_TWEAK "${CPACK_PACKAGE_VERSION_PATCH}")
else(FreeDV_VERSION_TWEAK)
    set(CPACK_VER_TWEAK "${CPACK_PACKAGE_VERSION_PATCH}.0")
endif(FreeDV_VERSION_TWEAK)
    
if(FREEDV_VERSION_TAG)
    if(FREEDV_HASH)
        set(CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH}-${FREEDV_VERSION_TAG}-${DATE_RESULT}-${FREEDV_HASH}")
    else(FREEDV_HASH)
        set(CPACK_PACKAGE_VERSION_PATCH "${CPACK_PACKAGE_VERSION_PATCH}-${FREEDV_VERSION_TAG}-${DATE_RESULT}")
    endif(FREEDV_HASH)
    message(STATUS "package name = ${CPACK_PACKAGE_VERSION_PATCH}")
endif()

# Ensures that sample wav files are installed along with the application
install(
    DIRECTORY wav
    DESTINATION share/freedv-gui
    FILES_MATCHING PATTERN "*.wav")

# Install documentation for the current version.
install(
    FILES USER_MANUAL.pdf USER_MANUAL.html USER_MANUAL.md
    DESTINATION share/freedv-gui
)    

if(WIN32)
    # Install pdb files.
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/src/freedv.pdb 
        DESTINATION bin
    )
endif(WIN32)

if(WIN32)
    #
    # Cpack NSIS configuration for Windows.
    #

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
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}\r\nDeleteRegKey HKLM 'SOFTWARE\\\\CODEC2-Project\\\\FreeDV'")
    set(CPACK_NSIS_MENU_LINKS "share/freedv-gui/USER_MANUAL.pdf" "FreeDV User Manual")

    set(CPACK_WIN_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_VER_TWEAK}")

    string(TIMESTAMP COPYRIGHT_YEAR "%Y")
    set(CPACK_NSIS_DEFINES
        "${CPACK_NSIS_DEFINES}
VIProductVersion \\\"${CPACK_WIN_VERSION}\\\"
VIFileVersion \\\"${CPACK_WIN_VERSION}\\\"
VIAddVersionKey \\\"ProductName\\\" \\\"FreeDV\\\"
VIAddVersionKey \\\"ProductVersion\\\" \\\"${CPACK_WIN_VERSION}\\\"
VIAddVersionKey \\\"Company\\\" \\\"FreeDV\\\"
VIAddVersionKey \\\"LegalCopyright\\\" \\\"Copyright (c) ${COPYRIGHT_YEAR} FreeDV\\\"
VIAddVersionKey \\\"FileDescription\\\" \\\"FreeDV - ${PROJECT_DESCRIPTION}\\\"
VIAddVersionKey \\\"FileVersion\\\" \\\"${CPACK_WIN_VERSION}\\\"
VIAddVersionKey \\\"GitHash\\\" \\\"${FREEDV_HASH}\\\"
    ")

    # Ensures that we don't need to constantly update the rc file on every new release.
    string(REPLACE "." "," VERSION_AS_RC ${CPACK_WIN_VERSION})
    configure_file(${CMAKE_SOURCE_DIR}/contrib/freedv.rc.in ${CMAKE_BINARY_DIR}/freedv.rc)

    if (FREEDV_USING_LLVM_MINGW)
        set(CPACK_PACKAGE_FILE_NAME "FreeDV-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-windows-${CMAKE_SYSTEM_PROCESSOR}")
    endif (FREEDV_USING_LLVM_MINGW)

    if(SIGN_WINDOWS_BINARIES)
        # Add finalize command for NSIS so the installer/uninstaller can be
        # signed too.
        set(CPACK_NSIS_FINALIZE_CMD "mv %1 %1.tmp; ${OSSLSIGNCODE_BIN} sign -pkcs11engine ${PKCS11_ENGINE} -pkcs11module ${PKCS11_MODULE} -pkcs11cert \\\"${PKCS11_CERTIFICATE}\\\" -key \\\"${PKCS11_KEY}\\\" -h ${SIGN_HASH} -comm -ts ${TIMESTAMP_SERVER} -ac ${INTERMEDIATE_CERT_FILE} -n \\\"FreeDV - ${PROJECT_DESCRIPTION}\\\" -i \\\"https://freedv.org/\\\" -in %1.tmp -out %1")
    endif(SIGN_WINDOWS_BINARIES)

    include(CPack)
elseif(UNIX AND NOT APPLE)
    # Linux packaging
    SET(CPACK_GENERATOR "DEB")
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Mooneer Salem <mooneer@gmail.com>") #required
    if(USE_PULSEAUDIO)
    SET(CPACK_DEBIAN_PACKAGE_DEPENDS "codec2 (>= 1.1.1), lpcnet (>= 0.5.0), libspeexdsp1 (>= 1.2~rc1.2-1+b2), libsamplerate0 (>= 0.1.9-2), libwxgtk3.0-gtk3-0v5  (>= 3.0.4+dfsg-3), libpulse0 (>= 14.2-2), libhamlib2 (>= 3.3-10build1), libasound2 (>= 1.1.8-1), libao4 (>= 1.2.2+20180113-1), libgsm1 (>= 1.0.18-2), libsndfile1 (>= 1.0.28-6)")
    else(USE_PULSEAUDIO)
    SET(CPACK_DEBIAN_PACKAGE_DEPENDS "codec2 (>= 1.1.1), lpcnet (>= 0.5.0), libspeexdsp1 (>= 1.2~rc1.2-1+b2), libsamplerate0 (>= 0.1.9-2), libwxgtk3.0-gtk3-0v5  (>= 3.0.4+dfsg-3), libportaudio2 (>= 19.6.0-1build1), libhamlib2 (>= 3.3-10build1), libasound2 (>= 1.1.8-1), libao4 (>= 1.2.2+20180113-1), libgsm1 (>= 1.0.18-2), libsndfile1 (>= 1.0.28-6)")
    endif(USE_PULSEAUDIO)

    SET(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    include(CPack)

endif(WIN32)

if(UNITTEST)
if (LINUX AND USE_PULSEAUDIO)
# The below tests are currently Linux-only due to a dependency on
# PulseAudio/pipewire.
macro(DefineAudioTest utName)
    add_test(NAME fullduplex_${utName} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_zeros.sh txrx ${utName}) 
    set_tests_properties(fullduplex_${utName} PROPERTIES PASS_REGULAR_EXPRESSION "Got (1|3) sync changes")
endmacro()

DefineAudioTest(RADE)
DefineAudioTest(700D)
DefineAudioTest(700E)
DefineAudioTest(1600)
endif (LINUX AND USE_PULSEAUDIO)
endif(UNITTEST)
