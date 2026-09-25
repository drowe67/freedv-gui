# Vendored SQLite amalgamation, used by the text messaging message store.
#
# Built as a static library with the feature set trimmed to what FreeDV needs;
# the compile definitions below are the documented "recommended" options for
# embedded use (see https://sqlite.org/compile.html).

add_library(sqlite3 STATIC sqlite3.c)

target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_USE_ALLOCA
)

target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# SQLite is third party code; FreeDV's -Werror flags do not apply to it.
if(NOT MSVC)
    target_compile_options(sqlite3 PRIVATE -w)
endif(NOT MSVC)

if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif(UNIX AND NOT APPLE)
