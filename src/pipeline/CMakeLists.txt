add_library(fdv_audio_pipeline STATIC
    AudioPipeline.cpp
    ComputeRfSpectrumStep.cpp
    EitherOrStep.cpp
    EqualizerStep.cpp
    ExclusiveAccessStep.cpp
    FreeDVReceiveStep.cpp
    FreeDVTransmitStep.cpp
    IPipelineStep.cpp
    LevelAdjustStep.cpp
    LinkStep.cpp
    MuteStep.cpp
    ParallelStep.cpp
    PlaybackStep.cpp
    RADEReceiveStep.cpp
    RADETransmitStep.cpp
    RecordStep.cpp
    ResampleStep.cpp
    ResamplePlotStep.cpp
    SpeexStep.cpp
    TapStep.cpp
    ToneInterfererStep.cpp
    TxRxThread.cpp
)

target_include_directories(fdv_audio_pipeline PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

add_dependencies(fdv_audio_pipeline rade opus)

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
