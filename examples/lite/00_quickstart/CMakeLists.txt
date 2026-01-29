# examples/lite/00_quickstart/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------

wirekrak_add_lite_example(wkl_00_quickstart_trade trade.cpp)
wirekrak_add_lite_example(wkl_00_quickstart_book book.cpp)
