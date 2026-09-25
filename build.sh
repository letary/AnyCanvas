#!/usr/bin/env bash
# Linux / macOS / WSL build of core + tools + tests.
#   ./build.sh            configure (once) + build + ctest
#   ./build.sh --update   rewrite the golden expectations (review the diff!)
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build-posix -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-posix
if [[ "${1:-}" == "--update" ]]; then
  ./build-posix/anycanvas-golden tests/golden --update
fi
ctest --test-dir build-posix --output-on-failure
