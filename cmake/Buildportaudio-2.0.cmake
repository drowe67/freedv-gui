#set(BUILD_SHARED_LIBS OFF CACHE STRING "Disable shared libraries for portaudio")
#set(PA_ENABLE_DEBUG_OUTPUT ON CACHE STRING "Enable debug output")

include(FetchContent)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.28.0")
    FetchContent_Declare(
        portaudio
        GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        GIT_TAG        master
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(portaudio)
else()
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
    endif()
endif()

list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${portaudio_BINARY_DIR})
list(APPEND FREEDV_LINK_LIBS portaudio)
list(APPEND FREEDV_STATIC_DEPS portaudio)

include_directories(${portaudio_SOURCE_DIR}/include)
