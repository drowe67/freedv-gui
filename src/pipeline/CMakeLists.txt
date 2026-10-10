add_library(fdv_audio_pipeline_extra STATIC
    BeepStep.cpp
    ComputeRfSpectrumStep.cpp
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
    add_dependencies(fdv_audio_pipeline_extra wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid)
    target_compile_definitions(fdv_audio_pipeline_extra PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio_pipeline_extra PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)

if(UNITTEST)
macro(DefinePipelineUnitTest utName)
    DefineFreeDVUnitTest(pipeline ${utName} fdv_audio_pipeline_extra fdv_audio_pipeline fdv_util codec2 ${FREEDV_LINK_LIBS})
    target_include_directories(${utName} PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
    set_target_properties(
        ${utName}
        PROPERTIES BUILD_RPATH "${CMAKE_BUILD_RPATH};${CODEC2_BUILD_DIR}/src;${rade_BINARY_DIR}/src")
endmacro()

DefinePipelineUnitTest(BeepStepTest)
DefinePipelineUnitTest(ComputeRfSpectrumStepTest)
DefinePipelineUnitTest(MixStepTest)
DefinePipelineUnitTest(ParallelStepTest)
DefinePipelineUnitTest(PlaybackStepTest)
DefinePipelineUnitTest(RecordStepTest)
if(BOOTSTRAP_WXWIDGETS)
    target_compile_definitions(RecordStepTest PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(RecordStepTest PRIVATE ${WXBUILD_INCLUDES})
    target_link_libraries(RecordStepTest PRIVATE wx::core wx::base)
endif(BOOTSTRAP_WXWIDGETS)

DefinePipelineUnitTest(EqualizerStepTest)
target_sources(EqualizerStepTest PRIVATE ../eq_design.cpp ../sox_biquad.c)
target_link_libraries(EqualizerStepTest PRIVATE fdv_sox)
endif(UNITTEST)
