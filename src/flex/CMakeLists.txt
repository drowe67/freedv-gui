# Only include subset of the pipeline code to avoid unnecessary dependencies (like wxWidgets)
set(PIPELINE_SUBSET 
    ../pipeline/AgcStep.cpp
    ../pipeline/AudioPipeline.cpp
    ../pipeline/IPipelineStep.cpp
    ../pipeline/LevelAdjustStep.cpp
    ../pipeline/RADEReceiveStep.cpp
    ../pipeline/RADETransmitStep.cpp
    ../pipeline/ResampleStep.cpp
    ../pipeline/SpeexStep.cpp
    ../pipeline/rade_text.c)

if(LINUX AND DBUS_FOUND)
set(RTKIT_FILES
    ../audio/rtkit.c)
endif()

add_executable(freedv-flex
    EXCLUDE_FROM_ALL         # Don't build by default
    main.cpp 
    FlexRealtimeHelper.cpp
    FlexTxRxThread.cpp 
    FlexKeyValueParser.cpp 
    FlexVitaTask.cpp
    FlexTcpTask.cpp
    ReportingController.cpp

    $<TARGET_OBJECTS:mimalloc-obj> 
    
    ${PIPELINE_SUBSET}
    ${RTKIT_FILES})
    
target_compile_definitions(freedv-flex PRIVATE -DDISABLE_UNIT_TEST)
target_link_libraries(freedv-flex PRIVATE git_version fdv_reporting fdv_util fdv_os_wrapper rade opus codec2 samplerate agc ebur128 speexdsp)

if(LINUX AND DBUS_FOUND)
    target_include_directories(freedv-flex PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(freedv-flex PRIVATE -DUSE_RTKIT)
    target_link_libraries(freedv-flex PRIVATE ${DBUS_LIBRARIES})
endif()
