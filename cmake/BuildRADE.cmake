if(WIN32)

# XXX only x86_64 supported for now
set(PYTHON_URL https://www.python.org/ftp/python/3.12.6/python-3.12.6-embed-amd64.zip)
set(PYTHON_HASH ae256f31ee4700eba679802233bff3e9)

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

endif(WIN32)
