add_executable(zmq_hub src/main.cpp)
target_link_libraries(zmq_hub PRIVATE mp::core)
