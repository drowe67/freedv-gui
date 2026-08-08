add_library(fdv_audio_pipeline_extra STATIC
    BeepStep.cpp
    ComputeRfSpectrumStep.cpp
    DebugRecordStep.cpp
    EqualizerStep.cpp
    FreeDVReceiveStep.cpp
    FreeDVTransmitStep.cpp
    MixStep.cpp
    ParallelStep.cpp
    PlaybackStep.cpp
    RecordStep.cpp
    ResamplePlotStep.cpp
    TxRxThread.cpp
)

target_compile_options(fdv_audio_pipeline_extra PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_audio_pipeline_extra PUBLIC fdv_audio_pipeline fdv_util samplerate rnnoise_inc)
target_include_directories(fdv_audio_pipeline_extra PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_audio_pipeline_extra wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_audio_pipeline_extra PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio_pipeline_extra PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
