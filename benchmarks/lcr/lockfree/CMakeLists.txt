# benchmarks/lcr/lockfree/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------
add_executable(lcr_lookfree_spsc_queue spsc_queue.cpp)
target_link_libraries(lcr_lookfree_spsc_queue PRIVATE wirekrak)

add_executable(lcr_lookfree_spsc_ring spsc_ring.cpp)
target_link_libraries(lcr_lookfree_spsc_ring PRIVATE wirekrak)

add_executable(lcr_lookfree_spsc_compare_stack spsc_compare_stack.cpp)
target_link_libraries(lcr_lookfree_spsc_compare_stack PRIVATE wirekrak)

add_executable(lcr_lookfree_spsc_compare_heap spsc_compare_heap.cpp)
target_link_libraries(lcr_lookfree_spsc_compare_heap PRIVATE wirekrak)
