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

add_library(fdv_integ_audio_pipeline STATIC
    ${PIPELINE_SUBSET})

target_compile_definitions(fdv_integ_audio_pipeline PRIVATE -DDISABLE_UNIT_TEST)
target_link_libraries(fdv_integ_audio_pipeline PRIVATE rade opus codec2 samplerate agc ebur128 ${SPEEXDSP_LIBRARIES})
