cmake_minimum_required(VERSION 3.16)
project(mp_board LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# 平台选择：-DPLATFORM=host|mr527
set(PLATFORM "host" CACHE STRING "target platform")
set(PLATFORM_CMAKE "${CMAKE_SOURCE_DIR}/cmake/platforms/platform_${PLATFORM}.cmake")
if(EXISTS "${PLATFORM_CMAKE}")
    include("${PLATFORM_CMAKE}")
else()
    message(FATAL_ERROR "unsupported platform: ${PLATFORM}")
endif()

set(MP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MP_OUTPUT_DIR}/bin")

add_subdirectory(libs)
add_subdirectory(zmq_hub)
add_subdirectory(carrier)
add_subdirectory(network)
add_subdirectory(tasks)
add_subdirectory(monitor)
add_subdirectory(mpm)

message(STATUS "mp_board platform = ${PLATFORM}")
message(STATUS "output dir        = ${MP_OUTPUT_DIR}")
