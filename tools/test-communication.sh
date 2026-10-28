#!/bin/bash
set -euo pipefail
spring_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$spring_root"
mkdir -p tests/.build
for name in social_test mind_device_test character_test agency_test agency_flow_runner; do
  clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main "tests/spring/$name.cc" -o "tests/.build/$name"
  if [[ "$name" == character_test ]]; then
    "tests/.build/$name" tests/.build/v55-fixtures.json
  elif [[ "$name" == agency_flow_runner ]]; then
    "tests/.build/$name" fixture > tests/.build/v56-agency-flow-fixture.json
  else
    "tests/.build/$name"
  fi
done
python3 -m unittest discover -s robot-mind/tests -v
