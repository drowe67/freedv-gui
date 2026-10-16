add_library(fdv_audio_pipeline_extra STATIC
    BeepStep.cpp
    ComputeRfSpectrumStep.cpp
    EqualizerStep.cpp
    MixStep.cpp
    PlaybackStep.cpp
    RecordStep.cpp
    ResamplePlotStep.cpp
    TxRxThread.cpp

    # For waterfall view
    modem_stats.c
    kiss_fft.c
    kiss_fftr.c
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
    DefineFreeDVUnitTest(pipeline ${utName} fdv_audio_pipeline_extra fdv_audio_pipeline fdv_util ${FREEDV_LINK_LIBS})
    target_include_directories(${utName} PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

    # Some of the steps in fdv_audio_pipeline_extra use wxWidgets.
    FreeDVUnitTestUsesWxWidgets(${utName})
endmacro()

DefinePipelineUnitTest(BeepStepTest)
DefinePipelineUnitTest(ComputeRfSpectrumStepTest)
DefinePipelineUnitTest(MixStepTest)
DefinePipelineUnitTest(PlaybackStepTest)
DefinePipelineUnitTest(RecordStepTest)

DefinePipelineUnitTest(EqualizerStepTest)
target_sources(EqualizerStepTest PRIVATE ../eq_design.cpp ../sox_biquad.c)
target_link_libraries(EqualizerStepTest PRIVATE fdv_sox)

# ResampleForPlotStep calls resample_for_plot(), which lives in ../util.cpp;
# compile it in to test the real implementation (test/UtilStubs.h provides
# what util.cpp needs from the application's main.cpp/topFrame.cpp, and
# fdv_rig_control what it needs from the rig controllers).
macro(DefineUtilBackedPipelineUnitTest utName)
    DefinePipelineUnitTest(${utName})
    target_sources(${utName} PRIVATE ../util.cpp)
    target_link_libraries(${utName} PRIVATE fdv_rig_control)
endmacro()

# TxRxThread builds the full TX/RX pipeline, so its test also needs
# FreeDVInterface (only compiled into the app) and EqualizerStep's
# eq_design/sox_biquad dependencies.
DefineUtilBackedPipelineUnitTest(TxRxThreadTest)
target_sources(TxRxThreadTest PRIVATE ../freedv_interface.cpp ../eq_design.cpp ../sox_biquad.c)
target_link_libraries(TxRxThreadTest PRIVATE fdv_sox)
# Real speech for its round trip cases: the recording that comes with the
# RADE sources the build fetches (see freedv-backend's SpeechSample.h).
include(ExternalProject)
ExternalProject_Get_Property(build_rade SOURCE_DIR)
target_compile_definitions(TxRxThreadTest PRIVATE SPEECH_WAV_PATH="${SOURCE_DIR}/wav/all.wav")

# FreeDVInterface on its own.
DefineUtilBackedPipelineUnitTest(FreeDVInterfaceTest)
target_sources(FreeDVInterfaceTest PRIVATE ../freedv_interface.cpp)

DefineUtilBackedPipelineUnitTest(ResamplePlotStepTest)

# The rest of what can be tested in util.cpp without the application's windows.
DefineUtilBackedPipelineUnitTest(UtilTest)
endif(UNITTEST)
