add_library(fdv_reporting STATIC
    pskreporter.cpp
    pskreporter.h
    FreeDVReporter.cpp
    FreeDVReporter.h
)

if(SIOCLIENT_ADD_DEPENDENCY)
add_dependencies(fdv_reporting sioclient)
endif(SIOCLIENT_ADD_DEPENDENCY)

target_compile_options(fdv_reporting PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_reporting PRIVATE yyjson git_version)
