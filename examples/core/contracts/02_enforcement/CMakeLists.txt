# examples/core/contracts/02_enforcement/CMakeLists.txt


include(${PROJECT_SOURCE_DIR}/cmake/WirekrakExamples.cmake)


# -------------------------------------------------------------
# Examples
# -------------------------------------------------------------
wirekrak_add_core_example(wkc_contracts_02_enforcement_kraken_ping_pong kraken_ping_pong.cpp)
wirekrak_add_core_example(wkc_contracts_02_enforcement_kraken_replay kraken_replay.cpp CLI11::CLI11)
wirekrak_add_core_example(wkc_contracts_02_enforcement_kraken_subscription_ack kraken_subscription_ack.cpp CLI11::CLI11)
