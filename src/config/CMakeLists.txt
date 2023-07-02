add_library(fdv_config STATIC
    FreeDVConfiguration.cpp
)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_config wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_config PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_config PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)