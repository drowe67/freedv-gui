message(STATUS "Reporting...")
if(SIOCLIENT_ADD_DEPENDENCY)
    add_library(fdv_reporting STATIC
        pskreporter.cpp
        pskreporter.h
        FreeDVReporter.cpp
        FreeDVReporter.h
    )
    add_dependencies(fdv_reporting sioclient)
    message(STATUS "Reporting via pskreporter and FreeDVReporter.")
else(SIOCLIENT_ADD_DEPENDENCY)
    add_library(fdv_reporting STATIC
        pskreporter.cpp
        pskreporter.h
        FreeDVReporter_dummy.cpp
        FreeDVReporter.h
    )
    message(STATUS "Reporting via pskreporter only.")
endif(SIOCLIENT_ADD_DEPENDENCY)
