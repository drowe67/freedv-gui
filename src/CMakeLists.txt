set(FREEDV_SOURCES
    main.cpp
    topFrame.cpp
    sox_biquad.c
    util.cpp
    eq.cpp
    voicekeyer.cpp
    playrec.cpp
    ongui.cpp
    freedv_interface.cpp
)

set(FREEDV_LINK_LIBS_OSX
    "-framework AVFoundation"
)

# Compile FreeDV components
add_subdirectory(audio)
add_subdirectory(config)
add_subdirectory(gui)
add_subdirectory(os)
add_subdirectory(pipeline)
add_subdirectory(reporting)
add_subdirectory(rig_control)
add_subdirectory(sox)
add_subdirectory(util)

# WIN32 is needed for Windows GUI apps and is ignored for UNIX like systems.
# In addition, there are some required OSX-specific code files for platform specific handling.
if(APPLE)
    set(RES_FILES freedv.icns)
    set_source_files_properties(freedv.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(FreeDV MACOSX_BUNDLE ${FREEDV_SOURCES} ${RES_FILES})
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
if(NOT LPCNET_DISABLED)
set(LPCNET_LIB_NAME lpcnetfreedv)
else(NOT LPCNET_DISABLED)
set(LPCNET_LIB_NAME )
endif(NOT LPCNET_DISABLED)

if(APPLE)
target_link_libraries(FreeDV fdv_audio fdv_audio_pipeline fdv_config fdv_gui_controls fdv_gui_dialogs fdv_gui_util fdv_os_wrapper fdv_rig_control fdv_reporting fdv_sox fdv_util ${LPCNET_LIB_NAME} codec2 rade opus)
else(APPLE)
target_link_libraries(freedv fdv_audio fdv_audio_pipeline fdv_config fdv_gui_controls fdv_gui_dialogs fdv_gui_util fdv_os_wrapper fdv_rig_control fdv_reporting fdv_sox fdv_util ${LPCNET_LIB_NAME} codec2 rade opus)
endif(APPLE)

if(APPLE)
    if (BUILD_OSX_UNIVERSAL)
        add_dependencies(FreeDV build_opus_arm build_opus_x86 build_rade)
    else (BUILD_OSX_UNIVERSAL)
        add_dependencies(FreeDV build_opus build_rade)
    endif (BUILD_OSX_UNIVERSAL)
else(APPLE)
    add_dependencies(freedv build_opus build_rade)
endif(APPLE)

# Add build dependencies for internally built external libraries.
if(USE_INTERNAL_CODEC2)
    if(APPLE)
        add_dependencies(FreeDV build_codec2)
    else(APPLE)
        add_dependencies(freedv build_codec2)
    endif(APPLE)
endif()

# Add dependencies for other statically built libraries, if applicable.
if(FREEDV_STATIC_DEPS)
    if(APPLE)
    add_dependencies(FreeDV ${FREEDV_STATIC_DEPS})
    else(APPLE)
    add_dependencies(freedv ${FREEDV_STATIC_DEPS})
    endif(APPLE)
    add_dependencies(fdv_audio ${FREEDV_STATIC_DEPS})
    add_dependencies(fdv_audio_pipeline ${FREEDV_STATIC_DEPS})
    add_dependencies(fdv_sox ${FREEDV_STATIC_DEPS})
endif(FREEDV_STATIC_DEPS)

# Link other dependencies
if(APPLE)
target_link_libraries(FreeDV ${FREEDV_LINK_LIBS} ${FREEDV_LINK_LIBS_OSX})
elseif(WIN32)
target_link_libraries(freedv ${FREEDV_LINK_LIBS} ws2_32)
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
    if(NOT LPCNET_DISABLE)
        set(LPCNET_DYLIBBUNDLER_PATH ${LPCNET_BUILD_DIR}/src:)
        set(LPCNET_DYLIBBUNDLER_ARG -s ${LPCNET_BUILD_DIR}/src)
    endif(NOT LPCNET_DISABLE)

    add_custom_command(
        TARGET FreeDV
        POST_BUILD
        COMMAND rm -rf dist_tmp FreeDV.dmg || true
        COMMAND DYLD_LIBRARY_PATH=${CODEC2_BUILD_DIR}/src:${LPCNET_DYLIBBUNDLER_PATH}${portaudio_BINARY_DIR}:${samplerate_BINARY_DIR}/src:${Python3_ROOT_DIR}:${DYLD_LIBRARY_PATH} ${CMAKE_SOURCE_DIR}/macdylibbundler/dylibbundler ARGS -od -b -x FreeDV.app/Contents/MacOS/FreeDV -d FreeDV.app/Contents/libs -p @loader_path/../libs/ -i /usr/lib ${LPCNET_DYLIBBUNDLER_ARG} -s ${CODEC2_BUILD_DIR}/src -s ${CMAKE_BINARY_DIR}/LPCNet_build/src -s ${CMAKE_BINARY_DIR}/codec2_build/src -s ${portaudio_BINARY_DIR} -s ${samplerate_BINARY_DIR}/src -s ${rade_BINARY_DIR}/src -s ${Python3_ROOT_DIR}
        COMMAND cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/freedv.icns FreeDV.app/Contents/Resources
        COMMAND rm ARGS -rf FreeDV.app/Contents/Frameworks
        COMMAND mkdir ARGS FreeDV.app/Contents/Frameworks
        COMMAND cp ARGS -r ${Python3_ROOT_DIR}/../../../Python.framework FreeDV.app/Contents/Frameworks
        COMMAND install_name_tool ARGS -add_rpath @loader_path/../Frameworks/Python.framework FreeDV.app/Contents/libs/librade*.dylib
        COMMAND cp ARGS ../rade_src/radae_*e.py FreeDV.app/Contents/Resources
        COMMAND cp ARGS -r ../rade_src/radae FreeDV.app/Contents/Resources
        COMMAND cp ARGS -r ../rade_src/model19_check3 FreeDV.app/Contents/Resources
        COMMAND codesign --force --options runtime --timestamp --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/entitlements.plist --sign - ${CMAKE_CURRENT_BINARY_DIR}/FreeDV.app
        COMMAND mkdir dist_tmp
        COMMAND cp -r FreeDV.app dist_tmp 
        COMMAND hdiutil create -srcfolder dist_tmp/ -volname FreeDV -format UDZO -fs HFS+ ./FreeDV.dmg
        COMMAND rm -rf dist_tmp
    )
endif(APPLE)

# Custom commands for signing Windows binaries
if(SIGN_WINDOWS_BINARIES)
    # Add custom command to sign freedv.exe.
    add_custom_command(
        TARGET freedv 
        POST_BUILD
        COMMAND mv $<TARGET_FILE:freedv> $<TARGET_FILE:freedv>.tmp
        COMMAND ${OSSLSIGNCODE_BIN} ARGS sign -pkcs11engine ${PKCS11_ENGINE} -pkcs11module ${PKCS11_MODULE} -pkcs11cert "${PKCS11_CERTIFICATE}" -key "${PKCS11_KEY}" -h ${SIGN_HASH} -ts ${TIMESTAMP_SERVER} -comm -ac ${INTERMEDIATE_CERT_FILE} -n "FreeDV - ${PROJECT_DESCRIPTION}" -i "https://freedv.org/" -in $<TARGET_FILE:freedv>.tmp -out $<TARGET_FILE:freedv>
        COMMAND rm $<TARGET_FILE:freedv>.tmp
        VERBATIM)
endif(SIGN_WINDOWS_BINARIES)
