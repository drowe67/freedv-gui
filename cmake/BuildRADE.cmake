if(CMAKE_CROSSCOMPILING)
    set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DPython3_ROOT_DIR=${Python3_ROOT_DIR} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
elseif(APPLE)
    set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DPython3_ROOT_DIR=${Python3_ROOT_DIR})
endif()

if(BUILD_OSX_UNIVERSAL)
    set(RADE_CMAKE_ARGS ${RADE_CMAKE_ARGS} -DBUILD_OSX_UNIVERSAL=1)
endif(BUILD_OSX_UNIVERSAL)

include(ExternalProject)
ExternalProject_Add(build_rade
   SOURCE_DIR rade_src
   BINARY_DIR rade_build
   GIT_REPOSITORY https://github.com/drowe67/radae.git
   GIT_TAG main
   CMAKE_ARGS ${RADE_CMAKE_ARGS}
   #CMAKE_CACHE_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
   INSTALL_COMMAND ""
)

ExternalProject_Get_Property(build_rade BINARY_DIR)
ExternalProject_Get_Property(build_rade SOURCE_DIR)
add_library(rade SHARED IMPORTED)
add_dependencies(rade build_rade)
include_directories(${SOURCE_DIR}/src)

set_target_properties(rade PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/src/librade${CMAKE_SHARED_LIBRARY_SUFFIX}"
    IMPORTED_IMPLIB   "${BINARY_DIR}/src/librade${CMAKE_IMPORT_LIBRARY_SUFFIX}"
)
list(APPEND FREEDV_PACKAGE_SEARCH_PATHS ${BINARY_DIR}/src)
set(rade_BINARY_DIR ${BINARY_DIR})

if(WIN32)

# XXX only x86_64 supported for now
set(PYTHON_URL https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-amd64.zip)
set(PYTHON_HASH 4c0a5a44d4ca1d0bc76fe08ea8b76adc)

# Download Python. This is only included in the installer.
FetchContent_Declare(download_python3 
    URL ${PYTHON_URL} 
    URL_HASH MD5=${PYTHON_HASH})
if (NOT download_python3_POPULATED)
    FetchContent_Populate(download_python3)
endif (NOT download_python3_POPULATED)

# Also download pip so we can install RADE dependencies after
# program installation.
FetchContent_Declare(download_pip
    URL https://bootstrap.pypa.io/get-pip.py
    DOWNLOAD_NO_EXTRACT TRUE)
if (NOT download_pip_POPULATED)
    FetchContent_Populate(download_pip)
endif (NOT download_pip_POPULATED)

# Per https://bnikolic.co.uk/blog/python/2022/03/14/python-embedwin.html
# there are some tweaks we need to make to the embeddable package first
# before FreeDV can use it. Additionally, renaming python312._pth does not
# allow pip to work; additional content should be added to this file instead
# (see example at https://github.com/sergeyyurkov1/make_portable_python_env/blob/master/make_portable_python_env.bat).
file(MAKE_DIRECTORY ${download_python3_SOURCE_DIR}/DLLs)
file(WRITE ${download_python3_SOURCE_DIR}/python312._pth "Lib/site-packages\r\npython312.zip\r\n.\r\n\r\n# Uncomment to run site.main() automatically\r\nimport site")

# Tell NSIS to install Python and pip into the same folder freedv.exe is in.
# This makes looking for python3.dll easier later.
# Note: the trailing slash below is needed to make sure a "download_python3"
# directory isn't created.
install( 
    DIRECTORY ${download_python3_SOURCE_DIR}/
    DESTINATION bin)
install(
    FILES ${download_pip_SOURCE_DIR}/get-pip.py
    DESTINATION bin)

# Install files needed for post-install to work.
install(
    FILES ${CMAKE_SOURCE_DIR}/cmake/rade-requirements.txt
    DESTINATION bin)
install(
    FILES ${CMAKE_SOURCE_DIR}/cmake/rade-setup.bat
    DESTINATION bin)

# Install RADE Python files
install(
    DIRECTORY ${SOURCE_DIR}/radae
    DESTINATION bin)
install(
    FILES ${BINARY_DIR}/src/radae_rx.exe
    DESTINATION bin)
install(
    FILES ${BINARY_DIR}/src/radae_tx.exe
    DESTINATION bin)
install(
    FILES ${SOURCE_DIR}/radae_txe.py ${SOURCE_DIR}/radae_rxe.py
    DESTINATION bin)
install(
    FILES ${SOURCE_DIR}/model19_check3/checkpoints/checkpoint_epoch_100.pth
    DESTINATION bin/model19_check3/checkpoints
)

# Ensure that rade-setup.bat is executed by the installer,
# otherwise no packages will be installed.
set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
    "ExecShellWait '' '\$INSTDIR\\\\bin\\\\rade-setup.bat' ''")

# Make sure we fully clean up after Python on uninstall.
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
    "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}\r\nRMDir /r /REBOOTOK '\$INSTDIR\\\\bin\\\\Lib'\r\nRMDir /r /REBOOTOK '\$INSTDIR\\\\bin\\\\scripts'\r\nRMDir /r /REBOOTOK '\$INSTDIR\\\\bin\\\\share'")

endif(WIN32)
