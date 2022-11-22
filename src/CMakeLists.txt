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
        MACOSX_BUNDLE_BUNDLE_VERSION "${FreeDV_VERSION}"
        MACOSX_BUNDLE_BUNDLE_SHORT_VERSION_STRING "${FreeDV_VERSION}"
        MACOSX_BUNDLE_BUNDLE_LONG_VERSION_STRING "${FreeDV_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "freedv"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/src/info.plist.in
    )
else()
    add_executable(freedv WIN32 ${FREEDV_SOURCES} ${RES_FILES})
endif(APPLE)

# Link imported or build tree targets.
if(APPLE)
target_link_libraries(FreeDV fdv_audio fdv_audio_pipeline fdv_sox lpcnetfreedv codec2)
else(APPLE)
target_link_libraries(freedv fdv_audio fdv_audio_pipeline fdv_sox lpcnetfreedv codec2)
endif(APPLE)

# Add build dependencies for internally built external libraries.
if(USE_INTERNAL_CODEC2)
    if(APPLE)
        add_dependencies(FreeDV build_codec2 build_lpcnetfreedv)
    else(APPLE)
        add_dependencies(freedv build_codec2 build_lpcnetfreedv)
    endif(APPLE)
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
target_link_libraries(freedv ${FREEDV_LINK_LIBS} ws2_32 usb-1.0)
else()
target_link_libraries(freedv ${FREEDV_LINK_LIBS})
endif(APPLE)

if(BOOTSTRAP_WXWIDGETS)
    if(APPLE)
        target_compile_definitions(FreeDV PRIVATE ${WXBUILD_BUILD_DEFS})
        target_include_directories(FreeDV PRIVATE ${WXBUILD_INCLUDES})
        
        add_dependencies(FreeDV wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
        target_link_libraries(FreeDV wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid)
    else(APPLE)
        target_compile_definitions(freedv PRIVATE ${WXBUILD_BUILD_DEFS})
        target_include_directories(freedv PRIVATE ${WXBUILD_INCLUDES})
        
        add_dependencies(freedv wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
        target_link_libraries(freedv wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid)
    endif(APPLE)
endif(BOOTSTRAP_WXWIDGETS)

# For older Xcode (< 9.0), bypass usage of @available.
if(APPLE)
if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0.0.900037)
add_definitions(-DAPPLE_OLD_XCODE)
endif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0.0.900037)
endif(APPLE)

# If a release tag's set, pass it to the compiler.
if(FREEDV_VERSION_TAG)
add_definitions(-DFREEDV_VERSION_TAG="${FREEDV_VERSION_TAG}")
endif(FREEDV_VERSION_TAG)

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
        COMMAND DYLD_LIBRARY_PATH=${CODEC2_BUILD_DIR}/src:${LPCNET_BUILD_DIR}/src:${DYLD_LIBRARY_PATH} ${CMAKE_SOURCE_DIR}/macdylibbundler/dylibbundler ARGS -od -b -x FreeDV.app/Contents/MacOS/FreeDV -d FreeDV.app/Contents/libs -p @loader_path/../libs/ -i /usr/lib -s ${LPCNET_BUILD_DIR}/src -s ${CODEC2_BUILD_DIR}/src -s ${CMAKE_BINARY_DIR}/LPCNet_build/src -s ${CMAKE_BINARY_DIR}/codec2_build/src
        COMMAND cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/freedv.icns FreeDV.app/Contents/Resources
        COMMAND mkdir dist_tmp
        COMMAND cp -r FreeDV.app dist_tmp
        COMMAND hdiutil create -srcfolder dist_tmp/ -volname FreeDV -format UDZO -fs HFS+ ./FreeDV.dmg
        COMMAND rm -rf dist_tmp
    )
endif(APPLE)
