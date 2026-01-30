# examples/lite/01_subscriptions/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------

wirekrak_add_lite_example(wkl_01_subscriptions_trade trade.cpp CLI11::CLI11)
wirekrak_add_lite_example(wkl_01_subscriptions_book book.cpp CLI11::CLI11)
