set(PORTAUDIO_TARBALL "pa_stable_v190600_20161030")

# required linking libraries on linux. Not sure about windows.
find_library(ALSA_LIBRARIES asound)

if(UNIX AND NOT ALSA_LIBRARIES)
    message(ERROR "Could not find alsa library which is required for portaudio.
On Linux systems try installing:
    alsa-lib-devel  (RPM based systems)
    libasound2-dev  (DEB based systems)"
    )
endif()

# Make sure that configure knows what system we're using when cross-compiling.
if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --target=${HOST} --enable-cxx --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
else()
    set(CONFIGURE_COMMAND ./configure --enable-cxx --without-jack --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
endif()

include(ExternalProject)
ExternalProject_Add(portaudio
    URL http://www.portaudio.com/archives/${PORTAUDIO_TARBALL}.tgz
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)
if(WIN32)
    find_library(WINMM winmm)
    set(PORTAUDIO_LIBRARIES 
        ${CMAKE_BINARY_DIR}/external/dist/lib/libportaudio.a
        ${CMAKE_BINARY_DIR}/external/dist/lib/libportaudiocpp.a
        ${WINMM}
)
else(WIN32)
    find_library(RT rt)
    find_library(ASOUND asound)
    set(PORTAUDIO_LIBRARIES
        ${CMAKE_BINARY_DIR}/external/dist/lib/libportaudio.a
        ${CMAKE_BINARY_DIR}/external/dist/lib/libportaudiocpp.a
        ${RT}
        ${ASOUND}
    )
endif(WIN32)
include_directories(${CMAKE_BINARY_DIR}/external/dist/include)

# Add the portaudio library to the list of libraries that must be linked.
list(APPEND FREEDV_LINK_LIBS ${PORTAUDIO_LIBRARIES})

# Setup a dependency so that this gets built before linking to freedv.
list(APPEND FREEDV_STATIC_DEPS portaudio)
