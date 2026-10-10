add_library(fdv_gui_util STATIC
    LabelOverrideAccessible.cpp
    NameOverrideAccessible.cpp
    TabLayoutSerializer.cpp
    wxListViewComboPopup.cpp
    WindowPositionRestore.cpp)

target_compile_options(fdv_gui_util PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_gui_util PUBLIC fdv_util)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_util wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid)
    target_compile_definitions(fdv_gui_util PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_util PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)

if(UNITTEST)
# Header-only; only needs what HamlibRigController.h itself needs.
DefineFreeDVUnitTest(gui_util FrequencyOpsTest fdv_util)
if(HAMLIB_ADD_DEPENDENCY)
    add_dependencies(FrequencyOpsTest hamlib)
endif(HAMLIB_ADD_DEPENDENCY)

DefineFreeDVUnitTest(gui_util TabLayoutSerializerTest fdv_gui_util ${FREEDV_LINK_LIBS})
target_include_directories(TabLayoutSerializerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
FreeDVUnitTestUsesWxWidgets(TabLayoutSerializerTest wx::aui)
endif(UNITTEST)
