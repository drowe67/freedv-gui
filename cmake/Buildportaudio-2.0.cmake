#set(BUILD_SHARED_LIBS OFF CACHE STRING "Disable shared libraries for portaudio")
#set(PA_ENABLE_DEBUG_OUTPUT ON CACHE STRING "Enable debug output")

include(FetchContent)
FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    GIT_TAG        master
)

FetchContent_GetProperties(portaudio)
if(NOT portaudio_POPULATED)
  FetchContent_Populate(portaudio)
  add_subdirectory(${portaudio_SOURCE_DIR} ${portaudio_BINARY_DIR} EXCLUDE_FROM_ALL)
  list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${portaudio_BINARY_DIR})
endif()

list(APPEND FREEDV_LINK_LIBS PortAudio)
list(APPEND FREEDV_STATIC_DEPS PortAudio)

include_directories(${portaudio_SOURCE_DIR}/include)
