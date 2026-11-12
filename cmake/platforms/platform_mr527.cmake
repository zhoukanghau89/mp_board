# 全志 MR527：aarch64 交叉编译
# 用法：cmake -DPLATFORM=mr527 -DTOOLCHAIN_PREFIX=<工具链>/bin/aarch64-linux-gnu- ..
set(MP_PLATFORM_NAME "mr527")

if(NOT DEFINED MP_TOOLCHAIN_PREFIX)
  set(MP_TOOLCHAIN_PREFIX "aarch64-linux-gnu-" CACHE STRING "交叉工具链前缀")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER   "${MP_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${MP_TOOLCHAIN_PREFIX}g++")

# 目标板 sysroot 与库：交叉编译时 libzmq 需要目标架构版本，
# 建议把 mr527 的 libzmq 头文件与 .so 放到 third_party/zmq/<platform>/
set(MP_ZMQ_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/zmq/mr527/include")
set(MP_ZMQ_LIB_DIR     "${CMAKE_SOURCE_DIR}/third_party/zmq/mr527/lib")
