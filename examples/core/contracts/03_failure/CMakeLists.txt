# examples/core/contracts/03_failure/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Minimal connection examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_contracts_03_failure_kraken_rejection kraken_rejection.cpp CLI11::CLI11)
