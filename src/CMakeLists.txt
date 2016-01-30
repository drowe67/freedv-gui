set(FREEDV_SOURCES
    dlg_audiooptions.cpp
    dlg_filter.cpp
    dlg_options.cpp
    dlg_ptt.cpp
    dlg_plugin.cpp
    fdmdv2_main.cpp
    fdmdv2_pa_wrapper.cpp
    fdmdv2_plot.cpp
    fdmdv2_plot_scalar.cpp
    fdmdv2_plot_scatter.cpp
    fdmdv2_plot_spectrum.cpp
    fdmdv2_plot_waterfall.cpp
    hamlib.cpp
    topFrame.cpp
    sox_biquad.c
    comp.h
    dlg_audiooptions.h
    dlg_filter.h
    dlg_options.h
    dlg_ptt.h
    fdmdv2_defines.h
    fdmdv2_main.h
    fdmdv2_pa_wrapper.h
    fdmdv2_plot.h
    fdmdv2_plot_scalar.h
    fdmdv2_plot_scatter.h
    fdmdv2_plot_spectrum.h
    fdmdv2_plot_waterfall.h
    hamlib.h
    sox_biquad.h
    sox.h
    topFrame.h
    version.h
)

# WIN32 is needed for Windows GUI apps and is ignored for UNIX like systems.
add_executable(freedv WIN32 ${FREEDV_SOURCES} ${RES_FILES})
target_link_libraries(freedv ${FREEDV_LINK_LIBS})
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
if(FREEDV_STATIC_DEPS)
    add_dependencies(freedv ${FREEDV_STATIC_DEPS})
endif(FREEDV_STATIC_DEPS)
install(TARGETS freedv
    RUNTIME DESTINATION bin)

# Custom commands to build OSX images.
if(APPLE)
    add_custom_command(
        TARGET freedv
        POST_BUILD
        COMMAND mkdir ARGS -p FreeDV.app/Contents/MacOS
        COMMAND mkdir ARGS -p FreeDV.app/Contents/Resources/English.lproj
        COMMAND cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/info.plist FreeDV.app/Contents
        COMMAND cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/freedv.icns FreeDV.app/Contents/Resources
        COMMAND echo ARGS -n "APPL????" > FreeDV.app/Contents/PkgInfo
        COMMAND cp ARGS freedv FreeDV.app/Contents/MacOS/FreeDV
        COMMAND dylibbundler ARGS -od -b -x FreeDV.app/Contents/MacOS/FreeDV -d FreeDV.app/Contents/libs -p @executable_path/../libs/
        COMMAND mkdir dist_tmp
        COMMAND cp -r FreeDV.app dist_tmp
        COMMAND hdiutil create -srcfolder dist_tmp/ -volname FreeDV -format UDZO ./FreeDV.dmg
        COMMAND rm -rf dist_tmp
    )
endif(APPLE)
