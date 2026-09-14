add_library(fdv_rig_control_omnirig STATIC
    OmniRig_i.c
    OmniRigController.cpp
)

target_include_directories(fdv_rig_control_omnirig PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/../..)
target_compile_options(fdv_rig_control_omnirig PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

# OmniRig_i.c is MIDL-generated COM interface glue (GUID definitions via
# the standard EXTERN_C + __declspec(selectany) idiom), not hand-written
# code. GCC 13's mingw-w64 target warns on every one of these ("...
# initialized and declared 'extern'") with no dedicated -W flag to
# disable individually, so -Werror would need to reject vendored,
# unmodifiable boilerplate -- turn it back into a (harmless, ignorable)
# warning for this one generated file instead of relaxing it project-wide.
set_source_files_properties(OmniRig_i.c PROPERTIES COMPILE_OPTIONS "-Wno-error")

target_link_libraries(fdv_rig_control_omnirig PUBLIC fdv_util)
