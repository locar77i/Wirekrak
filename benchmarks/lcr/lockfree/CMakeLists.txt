# benchmarks/lcr/lockfree/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
add_executable(lcr_lookfree_spsc_queue spsc_queue.cpp)
target_link_libraries(lcr_lookfree_spsc_queue PRIVATE wirekrak)

add_executable(lcr_lookfree_spsc_ring spsc_ring.cpp)
target_link_libraries(lcr_lookfree_spsc_ring PRIVATE wirekrak)

add_executable(lcr_lookfree_spsc_compare spsc_compare.cpp)
target_link_libraries(lcr_lookfree_spsc_compare PRIVATE wirekrak)
