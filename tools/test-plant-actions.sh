#!/bin/bash
set -euo pipefail
plant_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$plant_root"
mkdir -p tests/.build evidence/plant-actions
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 \
  tests/spring/plant_action_test.cc -o tests/.build/plant-action-test
./tests/.build/plant-action-test
