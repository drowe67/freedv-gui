add_library(fdv_rig_control STATIC
    HamlibRigController.cpp
    SerialPortRigController.cpp
    SerialPortInRigController.cpp
    SerialPortOutRigController.cpp
)

target_include_directories(fdv_rig_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

if(HAMLIB_ADD_DEPENDENCY)
add_dependencies(fdv_rig_control hamlib)
endif(HAMLIB_ADD_DEPENDENCY)

if(WIN32)
# Add OmniRig support on Windows
add_subdirectory(omnirig)
target_link_libraries(fdv_rig_control PUBLIC fdv_rig_control_omnirig)
endif(WIN32)

target_link_libraries(fdv_rig_control PUBLIC fdv_util)
target_compile_options(fdv_rig_control PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

if(UNITTEST)
if(HAMLIB_ADD_DEPENDENCY)
    # Hamlib is being built as part of FreeDV.
    DefineFreeDVUnitTest(rig_control HamlibRigControllerTest fdv_rig_control hamlib)
else(HAMLIB_ADD_DEPENDENCY)
    DefineFreeDVUnitTest(rig_control HamlibRigControllerTest fdv_rig_control ${HAMLIB_LIBRARY})
endif(HAMLIB_ADD_DEPENDENCY)
target_include_directories(HamlibRigControllerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
endif(UNITTEST)
