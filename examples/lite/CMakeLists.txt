# examples/lite/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------

wirekrak_add_lite_example(wk_lite_minimal_trade_subscription minimal_trade_subscription.cpp)
wirekrak_add_lite_example(wk_lite_trade_subscription trade_subscription.cpp CLI11::CLI11)

wirekrak_add_lite_example(wk_lite_minimal_book_subscription minimal_book_subscription.cpp)
wirekrak_add_lite_example(wk_lite_book_subscription book_subscription.cpp CLI11::CLI11)
