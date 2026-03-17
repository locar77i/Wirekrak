# benchmarks/lcr/lockfree/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
add_executable(lcr_lookfree_spsc_ring spsc_ring.cpp)
target_link_libraries(lcr_lookfree_spsc_ring PRIVATE wirekrak)
