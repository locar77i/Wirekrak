# examples/core/transport/websocket/winhttp/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)



# -------------------------------------------------------------
# Transport winhttp
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_transport_websocket_winhttp_trade_subscription trade_subscription.cpp)
wirekrak_add_core_example(wkc_transport_websocket_winhttp_book_subscription book_subscription.cpp)
