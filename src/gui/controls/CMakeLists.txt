add_library(fdv_gui_controls STATIC
    plot.cpp
    plot_scalar.cpp
    plot_scatter.cpp
    plot_spectrum.cpp
    plot_waterfall.cpp
    ReportMessageRenderer.cpp)

target_include_directories(fdv_gui_controls PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/../..)
add_dependencies(fdv_gui_controls codec2)
target_compile_options(fdv_gui_controls PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_controls wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_gui_controls PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_controls PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
