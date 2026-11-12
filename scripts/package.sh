#!/usr/bin/env bash
# 打包：tar.gz + md5
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORM="${1:-host}"
BUILD_DIR="$ROOT/build/$PLATFORM/out/bin"
REL_DIR="$ROOT/release"
VER="$(git -C "$ROOT" describe --always --dirty 2>/dev/null || date +%Y%m%d%H%M)"
NAME="mp_board_${VER}_${PLATFORM}"

mkdir -p "$REL_DIR"
tar -czf "$REL_DIR/$NAME.tar.gz" -C "$BUILD_DIR" . -C "$ROOT" config
( cd "$REL_DIR" && md5sum "$NAME.tar.gz" > "$NAME.tar.gz.md5" )

echo "package: $REL_DIR/$NAME.tar.gz"
cat "$REL_DIR/$NAME.tar.gz.md5"
