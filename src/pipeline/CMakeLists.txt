add_library(fdv_audio_pipeline STATIC
    AudioPipeline.h
    AudioPipeline.cpp
    ComputeRfSpectrumStep.h
    ComputeRfSpectrumStep.cpp
    EitherOrStep.h
    EitherOrStep.cpp
    EqualizerStep.h
    EqualizerStep.cpp
    ExclusiveAccessStep.h
    ExclusiveAccessStep.cpp
    ExternVocoderStep.cpp
    FreeDVReceiveStep.h
    FreeDVReceiveStep.cpp
    FreeDVTransmitStep.h
    FreeDVTransmitStep.cpp
    IPipelineStep.h
    IPipelineStep.cpp
    LevelAdjustStep.h
    LevelAdjustStep.cpp
    LinkStep.h
    LinkStep.cpp
    MuteStep.h
    MuteStep.cpp
    ParallelStep.h
    ParallelStep.cpp
    PlaybackStep.h
    PlaybackStep.cpp
    RecordStep.h
    RecordStep.cpp
    ResampleStep.h
    ResampleStep.cpp
    ResamplePlotStep.h
    ResamplePlotStep.cpp
    SpeexStep.h
    SpeexStep.cpp
    TapStep.h
    TapStep.cpp
    ToneInterfererStep.h
    ToneInterfererStep.cpp
    TxRxThread.h
    TxRxThread.cpp
)

target_include_directories(fdv_audio_pipeline PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_audio_pipeline wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_audio_pipeline PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio_pipeline PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)

if(UNITTEST)
macro(DefineUnitTest utName)
    add_executable(${utName} test/${utName}.cpp)
    target_link_libraries(${utName} PRIVATE fdv_audio_pipeline)
    target_include_directories(${utName} PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
    
    add_test(NAME pipeline_${utName} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${utName})
endmacro()

DefineUnitTest(AudioPipelineTest)
target_link_libraries(AudioPipelineTest PRIVATE ${FREEDV_LINK_LIBS})
DefineUnitTest(EitherOrTest)
DefineUnitTest(ExclusiveAccessTest)
DefineUnitTest(LevelAdjustTest)
DefineUnitTest(ResampleTest)
target_link_libraries(ResampleTest PRIVATE ${FREEDV_LINK_LIBS})
DefineUnitTest(TapTest)
endif(UNITTEST)
