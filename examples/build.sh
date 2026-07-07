#!/usr/bin/env bash
# Build the hello example against ../mach.h -> build/hello
set -euo pipefail
cd "$(dirname "$0")"

case "$(uname -s)" in
  Darwin) libs="-framework Cocoa -framework CoreVideo -framework IOKit -framework OpenGL" ;;
  Linux)  libs="-lX11 -lXrandr -lGL -lm -ldl" ;;
  *) echo "Unsupported platform: $(uname -s) (on Windows, see the README's cl.exe line)" >&2; exit 1 ;;
esac

mkdir -p build
# shellcheck disable=SC2086
"${CC:-clang}" -std=c99 -Wall -Wextra -I.. -o build/hello hello.c $libs
echo "Built: examples/build/hello"
