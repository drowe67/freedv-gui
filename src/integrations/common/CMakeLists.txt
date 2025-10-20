set(PIPELINE_SUBSET
    ../../pipeline/AgcStep.cpp
    ../../pipeline/AudioPipeline.cpp
    ../../pipeline/IPipelineStep.cpp
    ../../pipeline/LevelAdjustStep.cpp
    ../../pipeline/RADEReceiveStep.cpp
    ../../pipeline/RADETransmitStep.cpp
    ../../pipeline/ResampleStep.cpp
    ../../pipeline/SpeexStep.cpp
    ../../pipeline/rade_text.c)

if(LINUX AND DBUS_FOUND)
set(RTKIT_FILES
    ../../audio/rtkit.c)
endif()

add_library(fdv_integ_audio_pipeline STATIC
    MinimalRealTimeHelper.cpp
    MinimalTxRxThread.cpp

    ${RTKIT_FILES}
    ${PIPELINE_SUBSET})

target_compile_definitions(fdv_integ_audio_pipeline PRIVATE -DDISABLE_UNIT_TEST)
target_link_libraries(fdv_integ_audio_pipeline PRIVATE rade opus codec2 samplerate agc ebur128 ${SPEEXDSP_LIBRARY})

if(LINUX AND DBUS_FOUND)
    target_include_directories(fdv_integ_audio_pipeline PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(fdv_integ_audio_pipeline  PRIVATE -DUSE_RTKIT)
    target_link_libraries(fdv_integ_audio_pipeline PRIVATE ${DBUS_LIBRARIES})
endif()
