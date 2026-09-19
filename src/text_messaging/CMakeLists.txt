add_library(fdv_text_messaging STATIC
    FrameCodec.cpp
)

target_include_directories(fdv_text_messaging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(fdv_text_messaging PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_text_messaging PUBLIC sqlite3)

if(UNITTEST)
    add_executable(fdv_text_messaging_test test/FrameCodecTest.cpp)
    target_link_libraries(fdv_text_messaging_test fdv_text_messaging)
    target_compile_options(fdv_text_messaging_test PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
    add_test(NAME text_messaging_frame_codec COMMAND fdv_text_messaging_test)
endif(UNITTEST)
