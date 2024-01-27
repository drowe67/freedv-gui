if(APPLE)
    set(OS_WRAP_FILES osx_interface.mm)
else(APPLE)
    set(OS_WRAP_FILES osx_stubs.cpp)
endif(APPLE)

if(WIN32)
    # Auto-update functionality requires winsparkle, so build the DLL for it.
    if(CMAKE_CROSSCOMPILING)
        set(WINSPARKLE_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    endif()

    # Build winsparkle library 
    include(ExternalProject)
    ExternalProject_Add(build_winsparkle
        SOURCE_DIR winsparkle_src
        SOURCE_SUBDIR cmake
        BINARY_DIR winsparkle_build
        INSTALL_DIR ${CMAKE_BINARY_DIR}/external/dist
        GIT_REPOSITORY https://github.com/vslavik/winsparkle.git
        GIT_TAG master
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external/dist ${WINSPARKLE_CMAKE_ARGS}
    )

    ExternalProject_Get_Property(build_winsparkle BINARY_DIR)
    ExternalProject_Get_Property(build_winsparkle SOURCE_DIR)
    add_library(winsparkle SHARED IMPORTED)

    set_target_properties(winsparkle PROPERTIES
        IMPORTED_LOCATION "${BINARY_DIR}/bin/WinSparkle${CMAKE_SHARED_LIBRARY_SUFFIX}"
        IMPORTED_IMPLIB   "${BINARY_DIR}/lib/libWinSparkle${CMAKE_IMPORT_LIBRARY_SUFFIX}"
    )

    set(WINSPARKLE_INCLUDE_DIRS ${SOURCE_DIR}/include)
    include_directories(${WINSPARKLE_INCLUDE_DIRS})

    # Add SW update related hooks.
    list(APPEND OS_WRAP_FILES windows_autoupdate.cpp)
    set(AUTOUPDATE_LIBRARY winsparkle)
    add_dependencies(winsparkle build_winsparkle)
elseif(APPLE)
    # TBD: Sparkle support for updates on macOS.
    list(APPEND OS_WRAP_FILES no_autoupdate.cpp)
else()
    list(APPEND OS_WRAP_FILES no_autoupdate.cpp)
endif()

add_library(fdv_os_wrapper STATIC
    ${OS_WRAP_FILES}
)

target_include_directories(fdv_os_wrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
target_link_libraries(fdv_os_wrapper PRIVATE ${AUTOUPDATE_LIBRARY})
