#!/bin/bash
set -euo pipefail
spring_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$spring_root"
mkdir -p tests/.build evidence/economy
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/economy_test.cc -o tests/.build/economy-test
./tests/.build/economy-test
bash tools/test-spring.sh
bash tools/test-touch.sh
./tests/.build/touch/spring_art_test evidence/economy
./tests/.build/touch/spring_economy_ui_test evidence/economy
bash tools/test-power.sh
clang++ -std=c++17 -O1 -Wall -Wextra -Werror -I vendor/xiaozhi-esp32/main tests/spring/economy_replay.cc -o tests/.build/economy-replay
./tests/.build/economy-replay > evidence/economy/replay.json
python3 - <<'PY'
import json
from pathlib import Path
records=json.loads(Path('evidence/economy/replay.json').read_text())
assert len(records)==21
for entry in records:
 state=entry['state'];e=state['economy'];r=state['resources']
 assert set(r)=={'water','energy','fiber','food','parts'}
 assert 0<=r['food']<=30 and 0<=r['parts']<=20 and 0<=r['water']<=100 and 0<=r['energy']<=100
 assert len(e['facilities'])==8 and len(state['plan_history'])<=12
Path('assets/art-v1/economy/replay-data.js').write_text('window.ECONOMY_REPLAY = '+json.dumps(records,ensure_ascii=False)+';\n')
print('Real-core replay: 21 frames and JSON/resource invariants passed')
PY
node --check assets/art-v1/economy/replay.js
node tests/economy-preview-test.cjs

clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 tests/spring/gardener_test.cc -o tests/.build/gardener-test
./tests/.build/gardener-test
python3 tests/hover-art-test.py

clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/economy_v3_test.cc -o tests/.build/economy-v3-test
./tests/.build/economy-v3-test

clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 tests/spring/life_test.cc -o tests/.build/life-test
./tests/.build/life-test > evidence/life-test.log
python3 - <<'PYLIFE'
import json
from pathlib import Path
rows=Path("evidence/life-test.log").read_text().splitlines()
state=json.loads(rows[-1]); assert len(state["life"]["history"])==6
assert not state["life"]["request_contact"]
print(rows[0])
PYLIFE

clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/memory_test.cc -o tests/.build/memory-test
./tests/.build/memory-test > evidence/memory-test.log
head -n 1 evidence/memory-test.log

clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 tests/spring/life_invitation_test.cc -o tests/.build/life-invitation-test
./tests/.build/life-invitation-test > evidence/life-invitation-test.log
cat evidence/life-invitation-test.log

clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main -I vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16 tests/spring/life_art_test.cc -o tests/.build/life-art-test
./tests/.build/life-art-test
