# Disable mimalloc if using sanitizers
if((LINUX) AND (NOT ENABLE_TSAN AND NOT ENABLE_ASAN AND NOT ENABLE_RTSAN))
#set(MIMALLOC_SOURCE_FILE $<TARGET_OBJECTS:mimalloc-obj>)
endif((LINUX) AND (NOT ENABLE_TSAN AND NOT ENABLE_ASAN AND NOT ENABLE_RTSAN))

add_executable(freedv-ka9q
    EXCLUDE_FROM_ALL         # Don't build by default
    main.cpp 
    ${MIMALLOC_SOURCE_FILE})
    
target_compile_options(freedv-ka9q PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_compile_definitions(freedv-ka9q PRIVATE -DFREEDV_INTEGRATION)
target_link_libraries(freedv-ka9q PRIVATE git_version fdv_integ_audio_pipeline fdv_reporting fdv_util fdv_os_wrapper)
