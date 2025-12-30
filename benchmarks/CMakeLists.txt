cmake_minimum_required(VERSION 3.25)

# Benchmarks are not part of the main library
# They are standalone executables

# -------------------------------------------------------------
# Add subdirectories
# -------------------------------------------------------------
add_subdirectory(transport)

# Future:
# add_subdirectory(protocol)
# add_subdirectory(stream)
