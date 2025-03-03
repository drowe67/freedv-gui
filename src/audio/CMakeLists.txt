if(USE_PULSEAUDIO AND LINUX)
set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
    PulseAudioDevice.cpp
    PulseAudioEngine.cpp
    )

else(USE_PULSEAUDIO AND LINUX)
set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
    PortAudioDevice.cpp
    PortAudioEngine.cpp
    PortAudioInterface.cpp
    )

endif(USE_PULSEAUDIO AND LINUX)

add_library(fdv_audio STATIC
    AudioDeviceSpecification.cpp
    AudioEngineFactory.cpp
    IAudioDevice.cpp
    IAudioEngine.cpp
    ${AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES}
)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_audio wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_audio PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
