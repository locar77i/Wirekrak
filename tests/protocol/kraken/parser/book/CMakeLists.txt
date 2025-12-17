# tests/protocol/kraken/parser/book/CMakeLists.txt

include(${PROJECT_SOURCE_DIR}/cmake/Wirekrak.TestHelpers.cmake)


file(GLOB BOOK_PARSER_TESTS test_*.cpp)

foreach(test_src ${BOOK_PARSER_TESTS})
    get_filename_component(test_name ${test_src} NAME_WE)
    wirekrak_add_parser_test(${test_name} ${test_src})
endforeach()
