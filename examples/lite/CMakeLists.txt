# examples/lite/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------

wirekrak_add_lite_example(wk_lite_trade_requests trade_requests.cpp CLI11::CLI11)
wirekrak_add_lite_example(wk_lite_trade_requests_minimal trade_requests_minimal.cpp)
