# benchmarks/lcr/buffer/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
add_executable(lcr_buffer_managed_spsc_ring_minimal managed_spsc_ring_minimal.cpp)
target_link_libraries(lcr_buffer_managed_spsc_ring_minimal PRIVATE wirekrak)

add_executable(lcr_buffer_managed_spsc_ring_minimal_ts managed_spsc_ring_minimal_ts.cpp)
target_link_libraries(lcr_buffer_managed_spsc_ring_minimal_ts PRIVATE wirekrak)

add_executable(lcr_buffer_managed_spsc_ring_realistic managed_spsc_ring_realistic.cpp)
target_link_libraries(lcr_buffer_managed_spsc_ring_realistic PRIVATE wirekrak)
