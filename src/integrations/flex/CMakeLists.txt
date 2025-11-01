# Disable mimalloc if using sanitizers
if((LINUX OR APPLE) AND (NOT ENABLE_TSAN AND NOT ENABLE_ASAN AND NOT ENABLE_RTSAN))
set(MIMALLOC_SOURCE_FILE $<TARGET_OBJECTS:mimalloc-obj>)
endif((LINUX OR APPLE) AND (NOT ENABLE_TSAN AND NOT ENABLE_ASAN AND NOT ENABLE_RTSAN))

add_executable(freedv-flex
    EXCLUDE_FROM_ALL         # Don't build by default
    main.cpp 
    FlexKeyValueParser.cpp 
    FlexVitaTask.cpp
    FlexTcpTask.cpp

    ${MIMALLOC_SOURCE_FILE})

target_compile_options(freedv-flex PRIVATE ${WARNINGS_AS_ERRORS_FLAGS} -Wno-missing-field-initializers)
target_link_libraries(freedv-flex PRIVATE fdv_integ_audio_pipeline fdv_reporting fdv_util)
