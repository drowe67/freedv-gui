add_library(fdv_rig_control_omnirig STATIC
    OmniRig_i.c
    OmniRigController.cpp
)

target_include_directories(fdv_rig_control_omnirig PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/../..)
target_compile_options(fdv_rig_control_omnirig PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
