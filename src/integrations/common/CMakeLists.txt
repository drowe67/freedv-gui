set(PIPELINE_SUBSET
    ../../pipeline/AgcStep.cpp
    ../../pipeline/AudioPipeline.cpp
    ../../pipeline/BandwidthExpandStep.cpp
    ../../pipeline/IPipelineStep.cpp
    ../../pipeline/LevelAdjustStep.cpp
    ../../pipeline/RADEReceiveStep.cpp
    ../../pipeline/RADETransmitStep.cpp
    ../../pipeline/ResampleStep.cpp
    ../../pipeline/RNNoiseStep.cpp
    ../../pipeline/rade_text.c)

if(LINUX AND DBUS_FOUND)
set(RTKIT_FILES
    ../../audio/rtkit.c)
endif()

add_library(fdv_integ_audio_pipeline STATIC
    EXCLUDE_FROM_ALL
    MinimalRealTimeHelper.cpp
    MinimalTxRxThread.cpp
    ReportingController.cpp

    ${RTKIT_FILES}
    ${PIPELINE_SUBSET})

target_compile_options(fdv_integ_audio_pipeline PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_compile_definitions(fdv_integ_audio_pipeline PRIVATE -DDISABLE_UNIT_TEST)
target_link_libraries(fdv_integ_audio_pipeline PRIVATE fdv_os_wrapper rade opus codec2 agc rnnoise ebur128 samplerate)

if(LINUX AND DBUS_FOUND)
    target_include_directories(fdv_integ_audio_pipeline PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(fdv_integ_audio_pipeline  PRIVATE -DUSE_RTKIT)
    target_link_libraries(fdv_integ_audio_pipeline PRIVATE ${DBUS_LIBRARIES})
endif()
