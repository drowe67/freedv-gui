set(PORTAUDIO_GIT "https://github.com/PortAudio/portaudio.git")

# required linking libraries on linux. Not sure about windows.
find_library(ALSA_LIBRARIES asound)

if(UNIX AND NOT APPLE AND NOT ALSA_LIBRARIES)
    message(ERROR "Could not find alsa library which is required for portaudio.
On Linux systems try installing:
    alsa-lib-devel  (RPM based systems)
    libasound2-dev  (DEB based systems)"
    )
endif()

# Make sure that configure knows what system we're using when cross-compiling.
if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --target=${HOST} --disable-cxx --disable-shared --with-winapi=wmme,directx --prefix=${CMAKE_BINARY_DIR}/external/dist)
elseif(APPLE)
if(BUILD_OSX_UNIVERSAL)
    set(CONFIGURE_COMMAND ./configure --enable-mac-universal --enable-cxx --enable-option-checking --without-alsa --without-jack --without-oss --without-asihpi --without-winapi --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9 LDFLAGS=-framework\ CoreServices\ -framework\ AudioUnit\ -framework\ CoreFoundation\ -framework\ AudioToolbox\ -framework\ CoreAudio)
else()
    set(CONFIGURE_COMMAND ./configure --enable-cxx --enable-option-checking --without-alsa --without-jack --without-oss --without-asihpi --without-winapi --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist CFLAGS=-g\ -O2\ -mmacosx-version-min=10.9 LDFLAGS=-framework\ CoreServices\ -framework\ AudioUnit\ -framework\ CoreFoundation\ -framework\ AudioToolbox\ -framework\ CoreAudio)
endif(BUILD_OSX_UNIVERSAL)
else()
    set(CONFIGURE_COMMAND ./configure --enable-cxx --without-jack --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
endif()

include(ExternalProject)
if(APPLE)
ExternalProject_Add(portaudio
    GIT_REPOSITORY ${PORTAUDIO_GIT}
    GIT_TAG origin/master
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)
else()
ExternalProject_Add(portaudio
    GIT_REPOSITORY ${PORTAUDIO_GIT}
    GIT_TAG origin/master
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)
endif()
set(PORTAUDIO_LIBRARIES ${CMAKE_BINARY_DIR}/external/dist/lib/libportaudio.a)

if(WIN32)
    find_library(WINMM winmm)
    find_library(DSOUND dsound)
    list(APPEND PORTAUDIO_LIBRARIES ${WINMM} ${DSOUND}
)
elseif(NOT APPLE)
    find_library(RT rt)
    find_library(ASOUND asound)
    list(APPEND PORTAUDIO_LIBRARIES ${RT} ${ASOUND}
    )
endif(WIN32)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)

# Add the portaudio library to the list of libraries that must be linked.
list(APPEND FREEDV_LINK_LIBS ${PORTAUDIO_LIBRARIES})
if (APPLE)
    list(APPEND FREEDV_LINK_LIBS 
        "-framework CoreServices"
        "-framework AudioUnit"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework CoreAudio")
endif()

# Setup a dependency so that this gets built before linking to freedv.
list(APPEND FREEDV_STATIC_DEPS portaudio)
