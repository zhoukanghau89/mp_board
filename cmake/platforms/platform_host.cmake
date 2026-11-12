# 本机调试：x86_64 / aarch64 Linux 原生编译
set(MP_PLATFORM_NAME "host")
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
