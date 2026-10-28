#include "scene_art.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <miniz.h>

namespace desk_gap {
namespace {
uint8_t* decoded = nullptr;
lv_image_dsc_t images[SCENE_ART_FRAME_COUNT] = {};
}  // namespace

bool InitializeSceneArt() {
    if (decoded)
        return true;
    auto* bytes = static_cast<uint8_t*>(
        heap_caps_malloc(SCENE_ART_RAW_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* decoder = static_cast<tinfl_decompressor*>(
        heap_caps_calloc(1, sizeof(tinfl_decompressor), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!bytes || !decoder) {
        heap_caps_free(bytes);
        heap_caps_free(decoder);
        ESP_LOGE("DeskGapArt", "Cannot allocate art pack (%u bytes in PSRAM)", SCENE_ART_RAW_BYTES);
        return false;
    }
    tinfl_init(decoder);
    size_t input_size = SCENE_ART_PACKED_BYTES, output_size = SCENE_ART_RAW_BYTES;
    const auto result =
        tinfl_decompress(decoder, scene_art_deflate, &input_size, bytes, bytes, &output_size,
                         TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    heap_caps_free(decoder);
    if (result != TINFL_STATUS_DONE || input_size != SCENE_ART_PACKED_BYTES ||
        output_size != SCENE_ART_RAW_BYTES) {
        heap_caps_free(bytes);
        ESP_LOGE("DeskGapArt", "Art decompression failed: %d, input=%u output=%u", int(result),
                 unsigned(input_size), unsigned(output_size));
        return false;
    }
    for (unsigned i = 0; i < SCENE_ART_FRAME_COUNT; ++i) {
        const auto& f = scene_art_frames[i];
        if (f.offset > output_size || f.size > output_size - f.offset || f.width % 2 != 0) {
            heap_caps_free(bytes);
            ESP_LOGE("DeskGapArt", "Invalid frame %u", i);
            return false;
        }
        images[i].header.magic = LV_IMAGE_HEADER_MAGIC;
        images[i].header.cf = f.alpha ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
        images[i].header.w = f.width;
        images[i].header.h = f.height;
        images[i].header.stride = f.width * 2;
        images[i].data_size = f.size;
        images[i].data = bytes + f.offset;
    }
    decoded = bytes;  // Immutable for the lifetime of the LVGL scene.
    ESP_LOGI("DeskGapArt", "ART_V2 ready frames=%u flash=%u psram=%u free_psram=%u",
             SCENE_ART_FRAME_COUNT, SCENE_ART_PACKED_BYTES, SCENE_ART_RAW_BYTES,
             unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    return true;
}

const lv_image_dsc_t* SceneBackground() { return decoded ? &images[0] : nullptr; }
const lv_image_dsc_t* SceneImage(SceneArtKind kind, unsigned frame, bool mirror) {
    if (!decoded || static_cast<unsigned>(kind) >= SCENE_ART_COUNT)
        return nullptr;
    const auto& group = scene_art_groups[kind];
    return &images[(mirror ? group.mirror : group.first) + frame % group.count];
}
}  // namespace desk_gap
