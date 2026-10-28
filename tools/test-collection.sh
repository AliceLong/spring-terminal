#!/bin/bash
set -euo pipefail
collection_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$collection_root"
mkdir -p tests/.build evidence/harvest-v56
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/collection_test.cc -o tests/.build/collection-test
./tests/.build/collection-test
collection_cmake="${CMAKE:-$(command -v cmake)}"
collection_ninja="${NINJA:-$(command -v ninja)}"
"$collection_cmake" -S tests/touch -B tests/.build/touch -G Ninja -DCMAKE_MAKE_PROGRAM="$collection_ninja" > evidence/harvest-v56/cmake.log
"$collection_cmake" --build tests/.build/touch --target spring_collection_ui_test -j 8
./tests/.build/touch/spring_collection_ui_test evidence/harvest-v56 | tee evidence/harvest-v56/host-memory.json
mkdir -p evidence/harvest-v56/isolated
./tests/.build/touch/spring_collection_ui_test evidence/harvest-v56/isolated isolate | tee evidence/harvest-v56/isolated-memory.json
mkdir -p evidence/harvest-v56/rgb565
COLLECTION_RGB565=1 ./tests/.build/touch/spring_collection_ui_test evidence/harvest-v56/rgb565 | tee evidence/harvest-v56/rgb565-memory.json
