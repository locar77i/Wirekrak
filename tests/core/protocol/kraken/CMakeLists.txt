# tests/protocol/kraken/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakTests.cmake)


file(GLOB PROTOCOL_KRAKEN_TESTS test_*.cpp)

foreach(test_src ${PROTOCOL_KRAKEN_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    wirekrak_add_test(${test_name} ${test_src})
endforeach()



# ADD SUBDIRS
add_subdirectory(schema)
add_subdirectory(parser)
add_subdirectory(channel)
