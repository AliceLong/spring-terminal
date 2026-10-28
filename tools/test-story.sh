#!/bin/bash
set -euo pipefail
story_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$story_root"
mkdir -p tests/.build evidence/story-v46
for name in story_test story_flow_test; do
  clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 "tests/spring/${name}.cc" -o "tests/.build/${name}"
done
./tests/.build/story_test
./tests/.build/story_flow_test evidence/story-v46/replay.json
bash tools/test-touch.sh
./tests/.build/touch/spring_story_ui_test evidence/story-v46
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/monitor_test.cc -o tests/.build/monitor-test
./tests/.build/monitor-test > evidence/monitor-host-snapshot.json
python3 -m unittest discover -s monitor -p 'test_*.py' -v
node --check monitor/app.js
