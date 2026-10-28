#!/bin/bash
set -euo pipefail
spring_root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$spring_root/tests/.build" "$spring_root/evidence"
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I "$spring_root/vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16" \
  "$spring_root/tests/power_test.cc" -o "$spring_root/tests/.build/spring-power-test"
"$spring_root/tests/.build/spring-power-test" | tee "$spring_root/evidence/power-state-test.log"
