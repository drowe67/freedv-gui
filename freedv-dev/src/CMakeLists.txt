set(FREEDV_SOURCES
    dlg_audiooptions.cpp
    dlg_filter.cpp
    dlg_options.cpp
    dlg_ptt.cpp
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
