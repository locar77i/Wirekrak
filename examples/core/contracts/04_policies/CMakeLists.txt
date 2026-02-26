# examples/core/contracts/04_policies/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_zero_tolerance_backpressure kraken_zero_tolerance_backpressure.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_strict_backpressure kraken_strict_backpressure.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_relaxed_backpressure kraken_relaxed_backpressure.cpp CLI11::CLI11)
