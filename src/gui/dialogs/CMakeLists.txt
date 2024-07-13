add_library(fdv_gui_dialogs STATIC
    dlg_audiooptions.cpp
    dlg_easy_setup.cpp
    dlg_filter.cpp
    dlg_options.cpp
    dlg_ptt.cpp
    freedv_reporter.cpp
    monitor_volume_adj.cpp)

target_include_directories(fdv_gui_dialogs PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/../..)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_dialogs wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_gui_dialogs PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_dialogs PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
