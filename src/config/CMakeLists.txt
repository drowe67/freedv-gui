add_library(fdv_config STATIC
    AudioConfiguration.cpp
    FilterConfiguration.cpp
    FreeDVConfiguration.cpp
    ReportingConfiguration.cpp
    RigControlConfiguration.cpp
    WxWidgetsConfigStore.cpp
)

target_include_directories(fdv_config PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
add_dependencies(fdv_config build_rade codec2)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_config wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_config PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_config PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
