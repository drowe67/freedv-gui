# Only include subset of the pipeline code to avoid unnecessary dependencies (like wxWidgets)
set(PIPELINE_SUBSET 
    ../pipeline/IPipelineStep.cpp
    ../pipeline/RADEReceiveStep.cpp
    ../pipeline/RADETransmitStep.cpp
    ../pipeline/ResampleStep.cpp
    ../pipeline/AudioPipeline.cpp
    ../pipeline/rade_text.c)

add_executable(freedv-flex
    main.cpp 
    FlexTxRxThread.cpp 
    FlexKeyValueParser.cpp 
    FlexVitaTask.cpp
    FlexTcpTask.cpp
    
    # TBD - freedv-flex faults on startup if mimalloc is included
    #$<TARGET_OBJECTS:mimalloc-obj> 
    
    ${PIPELINE_SUBSET})
    
target_compile_definitions(freedv-flex PRIVATE -DDISABLE_UNIT_TEST)
target_link_libraries(freedv-flex PRIVATE fdv_util rade opus codec2 samplerate)
