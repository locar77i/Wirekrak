# benchmarks/transport/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
add_executable(websocket_throughput websocket_throughput.cpp)
target_link_libraries(websocket_throughput PRIVATE wirekrak)
