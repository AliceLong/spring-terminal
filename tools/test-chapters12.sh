#!/bin/bash
set -euo pipefail
chapter_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$chapter_root"
mkdir -p tests/.build evidence/chapters12
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
 -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 \
 tests/spring/chapters12_test.cc -o tests/.build/chapters12-test
./tests/.build/chapters12-test
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
 -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 \
 tests/spring/scene_input_test.cc -o tests/.build/scene-input-test
./tests/.build/scene-input-test
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
 -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 \
 tests/spring/communication_route_test.cc -o tests/.build/communication-route-test
./tests/.build/communication-route-test
