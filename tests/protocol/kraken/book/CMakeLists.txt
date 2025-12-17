# tests/protocol/kraken/book/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/Wirekrak.TestHelpers.cmake)


file(GLOB PROTOCOL_KRAKEN_BOOK_TESTS test_*.cpp)

foreach(test_src ${PROTOCOL_KRAKEN_BOOK_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    wirekrak_add_parser_test(${test_name} ${test_src})
endforeach()


# ADD SUBDIRS
add_subdirectory(common)
