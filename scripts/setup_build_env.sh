#!/usr/bin/env bash
# 检查本机依赖，缺什么打印安装命令
set -u
miss=0

check() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "缺少 $1"
    miss=1
  fi
}

check cmake
check g++
check pkg-config

if ! pkg-config --exists libzmq; then
  echo "缺少 libzmq 开发包"
  miss=1
fi

if [ "$miss" -eq 0 ]; then
  echo "依赖齐全"
else
  echo "安装：sudo apt-get install -y build-essential cmake pkg-config libzmq3-dev"
fi
