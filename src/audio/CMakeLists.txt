if(NATIVE_AUDIO_AVAILABLE)
    if(APPLE)
        set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
            MacAudioEngine.cpp
            MacAudioDevice.mm
        )
    elseif(WIN32)
        set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
            WASPIAudioEngine.cpp
        )
    elseif(LINUX)
        set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
            PulseAudioDevice.cpp
            PulseAudioEngine.cpp
        )
    endif()
else()
    set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
        PortAudioDevice.cpp
        PortAudioEngine.cpp
        PortAudioInterface.cpp
    )
endif(NATIVE_AUDIO_AVAILABLE)

add_library(fdv_audio STATIC
    AudioDeviceSpecification.cpp
    AudioEngineFactory.cpp
    IAudioDevice.cpp
    IAudioEngine.cpp
    ${AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES}
)

if(APPLE AND NATIVE_AUDIO_AVAILABLE)
    target_link_libraries(fdv_audio
        "-framework AVFoundation"
        "-framework CoreAudio")
elseif(WIN32 AND NATIVE_AUDIO_AVAILABLE)
    target_link_libraries(fdv_audio PRIVATE uuid)
endif()

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_audio wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_audio PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
