cmake_minimum_required(VERSION 3.25)

# External integrations are not part of the main library
# They are standalone executables


find_package(lz4 CONFIG REQUIRED)
find_package(xxHash CONFIG REQUIRED)


# -------------------------------------------------------------
# Matching Engine (integration)
# -------------------------------------------------------------
add_subdirectory(matching_engine)
