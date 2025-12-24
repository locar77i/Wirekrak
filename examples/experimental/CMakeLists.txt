# examples/experimental/CMakeLists.txt


find_package(lz4 CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)


# -------------------------------------------------------------
# Matching Engine (experimental)
# -------------------------------------------------------------
add_subdirectory(matching_engine)
