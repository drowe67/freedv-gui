if(WIN32)
    set(UTIL_WIN32_FILES logging/libfmemopen.c)
endif(WIN32)

add_library(fdv_util STATIC
    ThreadedObject.cpp
    logging/ulog.c
    ${UTIL_WIN32_FILES}
)
