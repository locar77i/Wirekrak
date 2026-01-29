# examples/lite/03_lifecycle/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------

wirekrak_add_lite_example(wkl_03_lifecycle_graceful_exit graceful_exit.cpp)
