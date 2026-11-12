file(GLOB MP_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(mpcore STATIC ${MP_LIB_SOURCES})
target_include_directories(mpcore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/sdk/include")

if(MP_PLATFORM_NAME STREQUAL "host")
    target_include_directories(mpcore PUBLIC ${ZMQ_INCLUDE_DIRS})
    target_link_directories(mpcore PUBLIC ${ZMQ_LIBRARY_DIRS})
    target_link_libraries(mpcore PUBLIC ${ZMQ_LIBRARIES} pthread rt)
else()
    target_include_directories(mpcore PUBLIC ${MP_ZMQ_INCLUDE_DIR})
    target_link_directories(mpcore PUBLIC ${MP_ZMQ_LIB_DIR})
    target_link_libraries(mpcore PUBLIC zmq pthread rt)
endif()

add_library(mp::core ALIAS mpcore)
