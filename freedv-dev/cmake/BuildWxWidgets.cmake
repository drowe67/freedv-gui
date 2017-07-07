set(WXWIDGETS_TARBALL "wxWidgets-3.0.2")

# If we're cross-compiling then we need to set the target host manually.
if(MINGW AND CMAKE_CROSSCOMPILING)
    include(cmake/MinGW.cmake)
endif()

# If not cross-compiling then use the built-in makefile, otherwise use standard configure.
if(MINGW AND NOT CMAKE_CROSSCOMPILING)
#    set(CONFIGURE_COMMAND "true")
#    set(MAKE_COMMAND $(MAKE) -C build/msw -f makefile.gcc SHARED=0 UNICODE=1 BUILD=release PREFIX=${CMAKE_BINARY_DIR}/external/dist)
    set(CONFIGURE_COMMAND ./configure --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
	set(MAKE_COMMAND $(MAKE))
endif()

if(MINGW AND CMAKE_CROSSCOMPILING)
    set(CONFIGURE_COMMAND ./configure --build=${HOST} --host=${HOST} --target=${HOST} --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
	set(MAKE_COMMAND $(MAKE))
endif()

if(NOT MINGW)
    set(CONFIGURE_COMMAND ./configure --host=${HOST} --target=${HOST} --disable-shared --prefix=${CMAKE_BINARY_DIR}/external/dist)
	set(MAKE_COMMAND $(MAKE))
endif()

include(ExternalProject)
ExternalProject_Add(wxWidgets
    URL http://downloads.sourceforge.net/wxwindows/${WXWIDGETS_TARBALL}.tar.bz2
    BUILD_IN_SOURCE 1
    INSTALL_DIR external/dist
    CONFIGURE_COMMAND ${CONFIGURE_COMMAND}
    BUILD_COMMAND ${MAKE_COMMAND}
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
