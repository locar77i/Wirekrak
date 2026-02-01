# examples/core/contracts/00_visibility/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_contracts_00_visibility_kraken_liveness_timeout kraken_liveness_timeout.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_00_visibility_kraken_trade kraken_trade.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_00_visibility_kraken_book kraken_book.cpp CLI11::CLI11)
