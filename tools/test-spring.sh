#!/bin/bash
set -euo pipefail
spring_root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$spring_root/tests/.build" "$spring_root/evidence"
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -I "$spring_root/vendor/xiaozhi-esp32/main" "$spring_root/tests/spring/core_test.cc" \
  -o "$spring_root/tests/.build/spring-core-test"
"$spring_root/tests/.build/spring-core-test" > "$spring_root/evidence/core-json.jsonl"
python3 - "$spring_root/evidence/core-json.jsonl" <<'PY'
import json,sys
for line in open(sys.argv[1]): json.loads(line)
print('World state / events JSON valid')
PY
