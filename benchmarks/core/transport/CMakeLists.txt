# benchmarks/transport/CMakeLists.txt


# -------------------------------------------------------------
# Benchmarks
# -------------------------------------------------------------

# WinHTTP benchmark (Windows only)
if (WIN32)
    add_executable(websocket_winhttp_throughput websocket_throughput.cpp)
    target_link_libraries(websocket_winhttp_throughput
        PRIVATE
            wirekrak_backend_winhttp
    )
endif()

# Asio benchmark (Linux / optional on Windows)
if (UNIX OR WIN32)
    add_executable(websocket_asio_beast_throughput websocket_throughput.cpp)
    target_link_libraries(websocket_asio_beast_throughput
        PRIVATE
            wirekrak_backend_asio
    )
endif()
