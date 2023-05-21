
add_library(fdv_reporting STATIC
    pskreporter.cpp
    pskreporter.h
    FreeDVReporter.cpp
    FreeDVReporter.h
)

add_dependencies(fdv_reporting sioclient)
