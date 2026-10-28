#!/bin/bash
set -euo pipefail
spring_root="$(cd "$(dirname "$0")/.." && pwd)"
spring_cmake="${CMAKE:-$(command -v cmake)}"
spring_ninja="${NINJA:-$(command -v ninja)}"
"$spring_cmake" -S "$spring_root/tests/touch" -B "$spring_root/tests/.build/touch" -G Ninja -DCMAKE_MAKE_PROGRAM="$spring_ninja"
"$spring_cmake" --build "$spring_root/tests/.build/touch" -j 8
"$spring_root/tests/.build/touch/spring_touch_test"
