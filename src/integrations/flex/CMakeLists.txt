add_executable(freedv-flex
    EXCLUDE_FROM_ALL         # Don't build by default
    main.cpp 
    FlexKeyValueParser.cpp 
    FlexVitaTask.cpp
    FlexTcpTask.cpp)

target_compile_options(freedv-flex PRIVATE ${WARNINGS_AS_ERRORS_FLAGS} -Wno-missing-field-initializers)
target_compile_definitions(freedv-flex PRIVATE -DFREEDV_INTEGRATION)
target_link_libraries(freedv-flex PRIVATE fdv_integ_audio_pipeline fdv_reporting fdv_util)

# If on Linux, force static libstdc++. This is to prevent versioning
# problems when using AppImages.
if(LINUX AND STATIC_LIBGCC)
    target_link_options(freedv-flex PRIVATE -static-libgcc -static-libstdc++)
endif(LINUX AND STATIC_LIBGCC)
