set(WXWIDGETS_VERSION "3.1.4")
set(WXWIDGETS_TARBALL "wxWidgets-${WXWIDGETS_VERSION}")

# If we're cross-compiling then we need to set the target host manually.
if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
endif()

# If not cross-compiling then use the built-in makefile, otherwise use standard configure.
if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND ./configure --build=${HOST} --host=${HOST} --target=${HOST} --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
elseif(APPLE)
    set(CONFIGURE_COMMAND ./configure --disable-shared --with-osx_cocoa --enable-universal_binary=arm64e,x86_64 --with-macosx-version-min=10.10 --prefix=${CMAKE_BINARY_DIR}/external/dist --with-libjpeg=builtin --with-libpng=builtin --with-regex=builtin --with-libtiff=builtin CXXFLAGS=-stdlib=libc++\ -std=c++11\ -DWX_PRECOMP\ -O2\ -fno-strict-aliasing\ -fno-common)
else()
#    set(CONFIGURE_COMMAND "true")
#    set(MAKE_COMMAND $(MAKE) -C build/msw -f makefile.gcc SHARED=0 UNICODE=1 BUILD=release PREFIX=${CMAKE_BINARY_DIR}/external/dist)
    set(CONFIGURE_COMMAND ./configure --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
endif()

# I don't see why we need this...
#if(NOT MINGW)    
#    set(CONFIGURE_COMMAND ./configure --host=${HOST} --target=${HOST} --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
#endif()

include(ExternalProject)
ExternalProject_Add(wxWidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v${WXWIDGETS_VERSION}/${WXWIDGETS_TARBALL}.tar.bz2
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
)

ExternalProject_Get_Property(wxWidgets install_dir)
message(STATUS "wxWidgets install dir: ${install_dir}")
if(NOT WXCONFIG)
    set(WXCONFIG "${install_dir}/bin/wx-config")
endif()
if(EXISTS ${WXCONFIG})
    set(BS_WX_DONE TRUE)
endif()
