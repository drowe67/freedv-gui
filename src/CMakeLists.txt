set(FREEDV_SOURCES
    dlg_audiooptions.cpp
    dlg_filter.cpp
    dlg_options.cpp
    dlg_ptt.cpp
    main.cpp
    plot.cpp
    plot_scalar.cpp
    plot_scatter.cpp
    plot_spectrum.cpp
    plot_waterfall.cpp
    hamlib.cpp
    serialport.cpp
    topFrame.cpp
    sox_biquad.c
    sox_biquad.h
    comp.h
    dlg_audiooptions.h
    dlg_filter.h
    dlg_options.h
    dlg_ptt.h
    defines.h
    main.h
    plot.h
    plot_scalar.h
    plot_scatter.h
    plot_spectrum.h
    plot_waterfall.h
    hamlib.h
    topFrame.h
    version.h
    osx_interface.h
    pskreporter.cpp
    pskreporter.h
    udp.cpp
    util.cpp
    eq.cpp
    voicekeyer.cpp
    playrec.cpp
    ongui.cpp
    freedv_interface.cpp
    freedv_interface.h        
)

set(FREEDV_SOURCES_OSX
    osx_interface.mm
)

set(FREEDV_LINK_LIBS_OSX
    "-framework AVFoundation"
)

# Compile FreeDV components
add_subdirectory(audio)
add_subdirectory(pipeline)
add_subdirectory(sox)

# WIN32 is needed for Windows GUI apps and is ignored for UNIX like systems.
# In addition, there are some required OSX-specific code files for platform specific handling.
if(APPLE)
    set(RES_FILES freedv.icns)
    set_source_files_properties(freedv.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(FreeDV MACOSX_BUNDLE ${FREEDV_SOURCES} ${RES_FILES} ${FREEDV_SOURCES_OSX})
    set_target_properties(FreeDV PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER org.freedv.freedv
        MACOSX_BUNDLE_BUNDLE_NAME FreeDV
        MACOSX_BNUDLE_COPYRIGHT "Copyright (c) 2021 FreeDV"
        MACOSX_BUNDLE_BUNDLE_VERSION "${FREEDV_VERSION}"
        MACOSX_BUNDLE_BUNDLE_SHORT_VERSION_STRING "${FREEDV_VERSION}"
        MACOSX_BUNDLE_BUNDLE_LONG_VERSION_STRING "${FREEDV_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "freedv"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/src/info.plist.in
    )
else()
    add_executable(freedv WIN32 ${FREEDV_SOURCES} ${RES_FILES})
endif(APPLE)

# Link imported or build tree targets.
if(APPLE)
target_link_libraries(FreeDV codec2 lpcnetfreedv fdv_audio fdv_audio_pipeline fdv_sox)
else(APPLE)
target_link_libraries(freedv codec2 lpcnetfreedv fdv_audio fdv_audio_pipeline fdv_sox)
endif(APPLE)

# Add build dependencies for internally built external libraries.
if(USE_INTERNAL_CODEC2)
    add_dependencies(freedv build_codec2 build_lpcnetfreedv)
endif()

# Add dependencies for other statically built libraries, if applicable.
if(FREEDV_STATIC_DEPS)
    if(APPLE)
    add_dependencies(FreeDV ${FREEDV_STATIC_DEPS})
    else(APPLE)
    add_dependencies(freedv ${FREEDV_STATIC_DEPS})
    endif(APPLE)
endif(FREEDV_STATIC_DEPS)

# Link other dependencies
if(APPLE)
target_link_libraries(FreeDV ${FREEDV_LINK_LIBS} ${FREEDV_LINK_LIBS_OSX})
elseif(WIN32)
target_link_libraries(freedv ${FREEDV_LINK_LIBS} ws2_32)
else()
target_link_libraries(freedv ${FREEDV_LINK_LIBS})
endif(APPLE)

# For older Xcode (< 9.0), bypass usage of @available.
if(APPLE)
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0.0.900037)
add_definitions(-DAPPLE_OLD_XCODE)
endif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0.0.900037)
endif(APPLE)

# Insert source and generated header directories before other search directories.
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
install(TARGETS FreeDV
    BUNDLE DESTINATION bin
)
else(APPLE)
install(TARGETS freedv
    RUNTIME DESTINATION bin
)
endif(APPLE)

# Custom commands to build OSX images.
if(APPLE)
    add_custom_command(
        TARGET FreeDV
        POST_BUILD
        COMMAND rm -rf dist_tmp FreeDV.dmg || true
        COMMAND DYLD_LIBRARY_PATH=${CODEC2_BUILD_DIR}/src:${LPCNET_BUILD_DIR}/src:${DYLD_LIBRARY_PATH} ${CMAKE_SOURCE_DIR}/macdylibbundler/dylibbundler ARGS -od -b -x FreeDV.app/Contents/MacOS/FreeDV -d FreeDV.app/Contents/libs -p @loader_path/../libs/ -i /usr/lib -s ${LPCNET_BUILD_DIR}/src -s ${CODEC2_BUILD_DIR}/src 
        COMMAND cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/freedv.icns FreeDV.app/Contents/Resources
        COMMAND mkdir dist_tmp
        COMMAND cp -r FreeDV.app dist_tmp
        COMMAND hdiutil create -srcfolder dist_tmp/ -volname FreeDV -format UDZO -fs HFS+ ./FreeDV.dmg
        COMMAND rm -rf dist_tmp
    )
endif(APPLE)
