add_executable(network src/main.cpp src/mqtt_stub.cpp)
target_link_libraries(network PRIVATE mp::core)
