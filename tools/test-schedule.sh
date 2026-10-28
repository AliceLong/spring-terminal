#!/bin/bash
set -euo pipefail
schedule_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$schedule_root"
mkdir -p tests/.build evidence/schedule-stage01
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
 -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 \
 tests/spring/schedule_test.cc -o tests/.build/schedule-test
./tests/.build/schedule-test
