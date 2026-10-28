#!/usr/bin/env python3
"""Exporter postprocess: keep host pixels, map the device background from assets."""
from pathlib import Path
import re
import zlib

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / 'vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16'
NAME = 'spring_background.rgb565'


def externalize():
    path = BOARD / 'spring_art_data.cc'
    source = path.read_text()
    if 'LoadExternalBackground(' in source:
        return
    raw = (ROOT / 'assets/art-v1/device/Background.bin').read_bytes()
    match = re.search(r'alignas\(4\) const uint8_t data_Background\[\] = \{.*?\n\};', source, re.S)
    if not match:
        raise ValueError('Background array missing; run the current art exporter first')
    external_condition = 'defined(ESP_PLATFORM) && !defined(SPRING_BACKGROUND_EMBEDDED)'
    source = source[:match.start()] + '#if !(' + external_condition + ')\n' + match[0] + (
        '\n#else\nconstexpr const uint8_t* data_Background = nullptr;\n#endif'
    ) + source[match.end():]
    source = source.replace('#include "spring_art_data.h"',
                            '#include "spring_art_data.h"\n#ifdef ESP_PLATFORM\n#include "spring_external_art.h"\n#endif')
    # Only one descriptor needs a runtime pointer; keep the other 133 in read-only flash.
    source = source.replace('const Asset assets[] = {', '''#if defined(ESP_PLATFORM) && !defined(SPRING_BACKGROUND_EMBEDDED)
lv_image_dsc_t external_background = descriptors[static_cast<unsigned>(Id::Background)];
alignas(4) const uint8_t background_error_pixel[4] = {};
#endif
const Asset assets[] = {''')
    source = source.replace('    {&descriptors[0],0,0},', '''#if defined(ESP_PLATFORM) && !defined(SPRING_BACKGROUND_EMBEDDED)
    {&external_background,0,0},
#else
    {&descriptors[0],0,0},
#endif''')
    old = 'const Asset& Get(Id id) { return assets[static_cast<unsigned>(id)]; }'
    new = '''const Asset& Get(Id id) {
#if defined(ESP_PLATFORM) && !defined(SPRING_BACKGROUND_EMBEDDED)
    // Initialize on first scene access, after the flash/partition runtime is ready.
    static const bool loaded = [] {
        external_background.data =
            LoadExternalBackground("%s", %du, 0x%08xu);
        if (!external_background.data) {
            // A safe descriptor lets the scene display a recovery overlay without bad reads.
            external_background.header.w = external_background.header.h = 1;
            external_background.header.stride = 2;
            external_background.data_size = 2;
            external_background.data = background_error_pixel;
        }
        return true;
    }();
    (void)loaded;
#endif
    return assets[static_cast<unsigned>(id)];
}''' % (NAME, len(raw), zlib.crc32(raw))
    if old not in source:
        raise ValueError('Unexpected generated Get implementation')
    path.write_text(source.replace(old, new))


if __name__ == '__main__':
    externalize()
