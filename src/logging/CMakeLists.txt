add_library(fdv_logging STATIC
    WSJTXNetworkLogger.cpp
)

target_compile_options(fdv_logging PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_logging PRIVATE git_version)