add_library(fdv_gui_theme STATIC FreeDVTheme.cpp FreeDVTheme.h)

target_compile_options(fdv_gui_theme PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_theme wx::core wx::base)
    target_compile_definitions(fdv_gui_theme PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_theme PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
