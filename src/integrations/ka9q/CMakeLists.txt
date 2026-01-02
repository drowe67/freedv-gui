add_executable(freedv-ka9q
    EXCLUDE_FROM_ALL         # Don't build by default
    main.cpp)
    
target_compile_options(freedv-ka9q PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_compile_definitions(freedv-ka9q PRIVATE -DFREEDV_INTEGRATION)
target_link_libraries(freedv-ka9q PRIVATE git_version fdv_integ_audio_pipeline fdv_reporting fdv_util fdv_os_wrapper)

# If on Linux, force static libstdc++. This is to prevent versioning
# problems when using AppImages.
if(LINUX AND STATIC_LIBGCC)
    target_link_options(freedv-ka9q PRIVATE -static-libgcc -static-libstdc++)
endif(LINUX)
