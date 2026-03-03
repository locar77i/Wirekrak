# examples/core/contracts/04_policies/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_backpressure_zero_tolerance kraken_backpressure_zero_tolerance.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_backpressure_strict kraken_backpressure_strict.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_backpressure_relaxed kraken_backpressure_relaxed.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_symbol_limit_hard_enforcement kraken_symbol_limit_hard_enforcement.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_liveness_disabled_and_active kraken_liveness_disabled_and_active.cpp  CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_liveness_enabled_and_passive kraken_liveness_enabled_and_passive.cpp  CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_04_policies_kraken_liveness_enabled_and_active kraken_liveness_enabled_and_active.cpp  CLI11::CLI11)
