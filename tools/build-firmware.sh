#!/usr/bin/env bash
set -euo pipefail
spring_root="$(cd "$(dirname "$0")/.." && pwd)"
command -v idf.py >/dev/null || { echo 'Activate ESP-IDF v6.1 first. See README.md.' >&2; exit 1; }
cd "$spring_root/vendor/xiaozhi-esp32"
python3 scripts/build.py waveshare/esp32-s3-touch-amoled-2.16 --name esp32-s3-touch-amoled-2.16-story-v46 --language zh-CN
