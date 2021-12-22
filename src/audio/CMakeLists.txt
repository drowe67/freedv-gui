if(USE_PULSEAUDIO)
set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
    PulseAudioDevice.cpp
    PulseAudioEngine.cpp
    )

else(USE_PULSEAUDIO)
set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
    PortAudioDevice.cpp
    PortAudioEngine.cpp
    )

endif(USE_PULSEAUDIO)

add_library(fdv_audio STATIC
    AudioDeviceSpecification.cpp
    AudioEngineFactory.cpp
    IAudioDevice.cpp
    IAudioEngine.cpp
    ${AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES}
)
