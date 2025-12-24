add_library(fdv_gui_util STATIC
    LabelOverrideAccessible.cpp
    NameOverrideAccessible.cpp
    wxListViewComboPopup.cpp)

target_compile_options(fdv_gui_util PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
    
if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_util wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_gui_util PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_util PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
