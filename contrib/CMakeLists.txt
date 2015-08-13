# Install icons if we're on most *nix systems.
if(UNIX AND NOT APPLE)
    set(ICON_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor
        CACHE PATH "Prefix to use for installing icons.")
    install(FILES freedv48x48.png
        DESTINATION ${ICON_INSTALL_PREFIX}/48x48/apps
        RENAME freedv.png)
    install(FILES freedv64x64.png
        DESTINATION ${ICON_INSTALL_PREFIX}/64x64/apps
        RENAME freedv.png)
    install(FILES freedv128x128.png
        DESTINATION ${ICON_INSTALL_PREFIX}/128x128/apps
        RENAME freedv.png)
    install(FILES freedv256x256.png
        DESTINATION ${ICON_INSTALL_PREFIX}/256x256/apps
        RENAME freedv.png)

    set(DESKTOP_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/share/applications
        CACHE PATH "Location to install desktop files.")
    install(FILES freedv.desktop
        DESTINATION ${DESKTOP_INSTALL_DIR})
endif(UNIX AND NOT APPLE)
