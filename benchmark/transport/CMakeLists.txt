# benchmark/transport/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
add_executable(winhttp_websocket_throughput winhttp_websocket_throughput.cpp)
target_link_libraries(winhttp_websocket_throughput PRIVATE wirekrak)
