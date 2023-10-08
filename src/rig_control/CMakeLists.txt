add_library(fdv_rig_control STATIC
    HamlibRigController.cpp
)

target_include_directories(fdv_rig_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

if(HAMLIB_ADD_DEPENDENCY)
add_dependencies(fdv_rig_control hamlib)
endif(HAMLIB_ADD_DEPENDENCY)
