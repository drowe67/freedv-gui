set(WXWIDGETS_VERSION "3.1.5")
set(WXWIDGETS_TARBALL "wxWidgets-${WXWIDGETS_VERSION}")

if(MINGW AND CMAKE_CROSSCOMPILING)
	# If we're cross-compiling then we need to set the target host manually.
    include(cmake/MinGW.cmake)

    # Fedora MinGW defines this to the system MinGW root which will prevent
	# finding libraries in the build tree. Add / so that it also looks in the
	# build tree. This is specifically for bootstrapping wxWidgets.
    list(APPEND CMAKE_FIND_ROOT_PATH "/")

	# If not cross-compiling then use the built-in makefile, otherwise use standard configure.
    set(CONFIGURE_COMMAND ./configure --build=${BUILD} --host=${HOST} --target=${HOST} --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)

elseif(APPLE)

if(BUILD_OSX_UNIVERSAL)
    set(CONFIGURE_COMMAND ./configure --disable-shared --with-osx_cocoa --enable-universal_binary=x86_64,arm64 --with-macosx-version-min=10.11 --prefix=${CMAKE_BINARY_DIR}/external/dist --with-libjpeg=builtin --with-libpng=builtin --with-regex=builtin --with-libtiff=builtin --with-expat=builtin --with-libcurl=builtin --with-zlib=builtin CXXFLAGS=-stdlib=libc++\ -std=c++11\ -DWX_PRECOMP\ -O2\ -fno-strict-aliasing\ -fno-common)
else()
    set(CONFIGURE_COMMAND ./configure --disable-shared --with-macosx-version-min=10.10 --prefix=${CMAKE_BINARY_DIR}/external/dist --with-libjpeg=builtin --with-libpng=builtin --with-regex=builtin --with-libtiff=builtin --with-libcurl=builtin --with-zlib=builtin --with-expat=builtin CXXFLAGS=-stdlib=libc++\ -std=c++11\ -DWX_PRECOMP\ -O2\ -fno-strict-aliasing\ -fno-common)
endif(BUILD_OSX_UNIVERSAL)

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
