# General OS specific hooks
if(APPLE)
    set(OS_WRAP_FILES osx_interface.mm)
else(APPLE)
    set(OS_WRAP_FILES osx_stubs.cpp)
endif(APPLE)

# Autoupdate handling
if(WIN32)
    list(APPEND OS_WRAP_FILES windows_autoupdate.cpp)
elseif(APPLE)
    list(APPEND OS_WRAP_FILES macos_autoupdate.mm)
else()
    list(APPEND OS_WRAP_FILES no_autoupdate.cpp)
endif()

add_library(fdv_os_wrapper STATIC
    ${OS_WRAP_FILES}
)

include(ExternalProject)

if(WIN32)
    # Auto-update functionality requires winsparkle, so build the DLL for it.
    if(CMAKE_CROSSCOMPILING)
        set(WINSPARKLE_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
    endif()

    # Build winsparkle library 
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
    set(AUTOUPDATE_LIBRARY winsparkle)
    add_dependencies(winsparkle build_winsparkle)
elseif(APPLE)
    # Download Sparkle
    ExternalProject_Add(build_sparkle
        URL https://github.com/sparkle-project/Sparkle/releases/download/2.5.2/Sparkle-2.5.2.tar.xz
        SOURCE_DIR sparkle_src
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        UPDATE_COMMAND "" # Skip annoying updates for every build
        INSTALL_COMMAND ""
    )
    ExternalProject_Get_Property(build_sparkle SOURCE_DIR)
    add_library(sparkle SHARED IMPORTED)

    set(AUTOUPDATE_LIBRARY "-framework Sparkle")

    # Ensure that the compile step can find Sparkle.framework.
    target_compile_options(fdv_os_wrapper PRIVATE -F ${SOURCE_DIR})
    target_link_directories(fdv_os_wrapper PRIVATE ${SOURCE_DIR})
    add_dependencies(fdv_os_wrapper build_sparkle)
endif()

target_include_directories(fdv_os_wrapper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
target_link_libraries(fdv_os_wrapper PRIVATE ${AUTOUPDATE_LIBRARY})
