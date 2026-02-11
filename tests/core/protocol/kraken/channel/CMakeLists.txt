# tests/protocol/kraken/channel/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/WirekrakTests.cmake)


file(GLOB PROTOCOL_KRAKEN_CHANNEL_TESTS test_*.cpp)

foreach(test_src ${PROTOCOL_KRAKEN_CHANNEL_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    wirekrak_add_test(${test_name} ${test_src})
endforeach()
