add_library(fdv_text_messaging STATIC
    FrameCodec.cpp
    HeardStationList.cpp
    MessageStore.cpp
    TextMessagingProtocol.cpp
    TextMessagingSession.cpp
)

target_include_directories(fdv_text_messaging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(fdv_text_messaging PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_text_messaging PUBLIC sqlite3)

if(UNITTEST)
    add_executable(fdv_text_messaging_test test/FrameCodecTest.cpp)
    target_link_libraries(fdv_text_messaging_test fdv_text_messaging)
    target_compile_options(fdv_text_messaging_test PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
    add_test(NAME text_messaging_frame_codec COMMAND fdv_text_messaging_test)

    add_executable(fdv_text_messaging_store_test test/MessageStoreTest.cpp)
    target_link_libraries(fdv_text_messaging_store_test fdv_text_messaging)
    target_compile_options(fdv_text_messaging_store_test PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
    add_test(NAME text_messaging_store COMMAND fdv_text_messaging_store_test)

    add_executable(fdv_text_messaging_protocol_test test/TextMessagingProtocolTest.cpp)
    target_link_libraries(fdv_text_messaging_protocol_test fdv_text_messaging)
    target_compile_options(fdv_text_messaging_protocol_test PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
    add_test(NAME text_messaging_protocol COMMAND fdv_text_messaging_protocol_test)
endif(UNITTEST)
