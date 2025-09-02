# Only include subset of the pipeline code to avoid unnecessary dependencies (like wxWidgets)
set(PIPELINE_SUBSET 
    ../pipeline/IPipelineStep.cpp
    ../pipeline/RADEReceiveStep.cpp
    ../pipeline/RADETransmitStep.cpp
    ../pipeline/ResampleStep.cpp
    ../pipeline/AudioPipeline.cpp
    ../pipeline/LevelAdjustStep.cpp
    ../pipeline/rade_text.c)

if(LINUX AND DBUS_FOUND)
set(RTKIT_FILES
    ../audio/rtkit.c)
endif()

add_executable(freedv-flex
    main.cpp 
    FlexRealtimeHelper.cpp
    FlexTxRxThread.cpp 
    FlexKeyValueParser.cpp 
    FlexVitaTask.cpp
    FlexTcpTask.cpp
    
    # TBD - freedv-flex faults on startup if mimalloc is included
    #$<TARGET_OBJECTS:mimalloc-obj> 
    
    ${PIPELINE_SUBSET}
    ${RTKIT_FILES})
    
target_compile_definitions(freedv-flex PRIVATE -DDISABLE_UNIT_TEST)
target_link_libraries(freedv-flex PRIVATE fdv_util rade opus codec2 samplerate)

if(LINUX AND DBUS_FOUND)
    target_include_directories(freedv-flex PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(freedv-flex PRIVATE -DUSE_RTKIT)
    target_link_libraries(freedv-flex PRIVATE ${DBUS_LIBRARIES})
endif()
