# Disable mimalloc if using sanitizers
if((LINUX OR APPLE) AND (NOT ENABLE_TSAN AND NOT ENABLE_ASAN AND NOT ENABLE_RTSAN))
set(MIMALLOC_SOURCE_FILE $<TARGET_OBJECTS:mimalloc-obj>)
endif((LINUX OR APPLE) AND (NOT ENABLE_TSAN AND NOT ENABLE_ASAN AND NOT ENABLE_RTSAN))

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

    ${MIMALLOC_SOURCE_FILE}
    ${RTKIT_FILES})
    
target_link_libraries(freedv-flex PRIVATE git_version fdv_integ_audio_pipeline fdv_reporting fdv_util fdv_os_wrapper)

if(LINUX AND DBUS_FOUND)
    target_include_directories(freedv-flex PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(freedv-flex PRIVATE -DUSE_RTKIT)
    target_link_libraries(freedv-flex PRIVATE ${DBUS_LIBRARIES})
endif()
