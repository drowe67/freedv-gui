# - Try to find Portaudio
# Once done this will define
#
#  PORTAUDIO_FOUND - system has Portaudio
#  PORTAUDIO_INCLUDE_DIRS - the Portaudio include directory
#  PORTAUDIO_LIBRARIES - Link these to use Portaudio
#  PORTAUDIO_DEFINITIONS - Compiler switches required for using Portaudio
#  PORTAUDIO_VERSION - Portaudio version
#
#  Copyright (c) 2006 Andreas Schneider <mail@cynapses.org>
#
# Redistribution and use is allowed according to the terms of the New BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (PORTAUDIO_LIBRARIES AND PORTAUDIO_INCLUDE_DIRS)
  # in cache already
  set(PORTAUDIO_FOUND TRUE)
else (PORTAUDIO_LIBRARIES AND PORTAUDIO_INCLUDE_DIRS)
  if (NOT WIN32)
   include(FindPkgConfig)
   pkg_check_modules(PORTAUDIO portaudio-2.0)
  endif (NOT WIN32)

  if (PORTAUDIO_FOUND)
    set(PORTAUDIO_LIBRARY_DIR ${PORTAUDIO_LIBRARY_DIRS})
    set(PORTAUDIO_VERSION
      19
    )
  else (PORTAUDIO_FOUND)
    find_path(PORTAUDIO_INCLUDE_DIR
      NAMES
        portaudio.h
      PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        /sw/include
    )
   
    find_library(PORTAUDIO_LIBRARY
      NAMES
        portaudio
      PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /sw/lib
    )
   
    find_path(PORTAUDIO_LIBRARY_DIR
      NAMES
        portaudio
      PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /sw/lib
    )
   
    set(PORTAUDIO_INCLUDE_DIRS
      ${PORTAUDIO_INCLUDE_DIR}
    )
    set(PORTAUDIO_LIBRARIES
      ${PORTAUDIO_LIBRARY}
    )
   
    set(PORTAUDIO_LIBRARY_DIRS
      ${PORTAUDIO_LIBRARY_DIR}
    )
   
    set(PORTAUDIO_VERSION
      18
    )
   
    if (PORTAUDIO_INCLUDE_DIRS AND PORTAUDIO_LIBRARIES)
       set(PORTAUDIO_FOUND TRUE)
    endif (PORTAUDIO_INCLUDE_DIRS AND PORTAUDIO_LIBRARIES)
   
    if (PORTAUDIO_FOUND)
      set(portaudio_BINARY_DIR ${PORTAUDIO_LIBRARY_DIR})
      if (NOT PORTAUDIO_FIND_QUIETLY)
        message(STATUS "Found portaudio: ${PORTAUDIO_LIBRARIES}")
      endif (NOT PORTAUDIO_FIND_QUIETLY)
    else (PORTAUDIO_FOUND)
      if (PORTAUDIO_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find portaudio")
      endif (PORTAUDIO_FIND_REQUIRED)
    endif (PORTAUDIO_FOUND)
  endif (PORTAUDIO_FOUND)


  # show the PORTAUDIO_INCLUDE_DIRS and PORTAUDIO_LIBRARIES variables only in the advanced view
  mark_as_advanced(PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES)

endif (PORTAUDIO_LIBRARIES AND PORTAUDIO_INCLUDE_DIRS)

