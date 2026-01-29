# examples/core/contracts/01_responsability/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_contracts_01_responsability_kraken_trade_cli kraken_trade_cli.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_01_responsability_kraken_book_cli kraken_book_cli.cpp CLI11::CLI11)
