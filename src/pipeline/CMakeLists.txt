set(FDV_AUDIO_PIPELINE_EXTRA_SOURCES
    ComputeRfSpectrumStep.cpp
    EqualizerStep.cpp
    FreeDVReceiveStep.cpp
    FreeDVTransmitStep.cpp
    ParallelStep.cpp
    PlaybackStep.cpp
    RecordStep.cpp
    ResamplePlotStep.cpp
    TxRxThread.cpp
)

if(ENABLE_GNUPG_AUTH)
    list(APPEND FDV_AUDIO_PIPELINE_EXTRA_SOURCES FreeDVAuthStep.cpp)
endif()

add_library(fdv_audio_pipeline_extra STATIC ${FDV_AUDIO_PIPELINE_EXTRA_SOURCES})

target_compile_options(fdv_audio_pipeline_extra PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_audio_pipeline_extra PUBLIC fdv_audio_pipeline fdv_util samplerate rnnoise_inc)

if(ENABLE_GNUPG_AUTH)
    target_compile_definitions(fdv_audio_pipeline_extra PUBLIC ENABLE_GNUPG_AUTH)
    target_link_libraries(fdv_audio_pipeline_extra PUBLIC PkgConfig::GPGME)
    find_library(GCRYPT_LIBRARY gcrypt)
    if(GCRYPT_LIBRARY)
        target_link_libraries(fdv_audio_pipeline_extra PUBLIC ${GCRYPT_LIBRARY})
    endif()
endif()
target_include_directories(fdv_audio_pipeline_extra PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_audio_pipeline_extra wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_audio_pipeline_extra PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio_pipeline_extra PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
