# Source after setting IDF_PATH (or ESP_IDF_DIR) to an ESP-IDF v6.1 checkout.
if [ -n "${ESP_IDF_DIR:-}" ] && [ -z "${IDF_PATH:-}" ]; then export IDF_PATH="$ESP_IDF_DIR"; fi
if [ -z "${IDF_PATH:-}" ] || [ ! -f "$IDF_PATH/export.sh" ]; then
  echo 'Set IDF_PATH to ESP-IDF v6.1, then source tools/esp-idf-env.sh' >&2
  return 1 2>/dev/null || exit 1
fi
. "$IDF_PATH/export.sh"
