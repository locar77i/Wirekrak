# examples/lite/02_multistream/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------

wirekrak_add_lite_example(wkl_02_multistream_trades_and_book trades_and_book.cpp)
