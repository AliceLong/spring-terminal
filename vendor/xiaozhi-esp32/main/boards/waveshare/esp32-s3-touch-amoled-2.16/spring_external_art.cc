#include "spring_external_art.h"
#include <cstring>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"

namespace spring_art {
namespace {
constexpr const char* kTag = "SpringExternalArt";
BackgroundMemory memory;
// Leave room for speech, display caches and transient allocations; never use internal RAM.
constexpr size_t kPsramReserve = 2 * 1024 * 1024;
struct Header {
    uint32_t count, checksum, length;
};
struct Entry {
    char name[32];
    uint32_t bytes, offset;
    uint16_t width, height;
};
static_assert(sizeof(Header) == 12 && sizeof(Entry) == 44);

esp_err_t Map(const char* name, size_t bytes, uint32_t crc, const uint8_t** pixels) {
    const auto* partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "assets");
    ESP_RETURN_ON_FALSE(partition, ESP_ERR_NOT_FOUND, kTag, "Missing assets partition");
    Header header{};
    ESP_RETURN_ON_ERROR(esp_partition_read(partition, 0, &header, sizeof(header)), kTag,
                        "Read assets header");
    ESP_RETURN_ON_FALSE(header.count && header.count <= 256 &&
                            header.length <= partition->size - sizeof(Header) &&
                            header.length >= header.count * sizeof(Entry),
                        ESP_ERR_INVALID_SIZE, kTag, "Invalid assets table");
    const size_t base = sizeof(Header) + header.count * sizeof(Entry);
    for (unsigned i = 0; i < header.count; ++i) {
        Entry entry{};
        ESP_RETURN_ON_ERROR(esp_partition_read(partition, sizeof(Header) + i * sizeof(Entry),
                                               &entry, sizeof(entry)),
                            kTag, "Read asset entry");
        if (strncmp(entry.name, name, sizeof(entry.name)) != 0)
            continue;
        const uint64_t offset = uint64_t(base) + entry.offset + 2;
        ESP_RETURN_ON_FALSE(entry.bytes == bytes && entry.width == 480 && entry.height == 480 &&
                                offset % 4 == 0 && offset + bytes <= sizeof(Header) + header.length,
                            ESP_ERR_INVALID_SIZE, kTag, "Background dimensions or bounds mismatch");
        uint8_t marker[2]{};
        ESP_RETURN_ON_ERROR(esp_partition_read(partition, offset - 2, marker, 2), kTag,
                            "Read asset marker");
        ESP_RETURN_ON_FALSE(marker[0] == 'Z' && marker[1] == 'Z', ESP_ERR_INVALID_RESPONSE, kTag,
                            "Invalid asset marker");
        const size_t start = size_t(offset) & ~size_t(65535);
        const size_t delta = size_t(offset) - start;
        const void* mapped = nullptr;
        // Deliberately retained for the lifetime of the scene; no PSRAM pixel copy.
        esp_partition_mmap_handle_t handle{};
        ESP_RETURN_ON_ERROR(esp_partition_mmap(partition, start, delta + bytes,
                                               ESP_PARTITION_MMAP_DATA, &mapped, &handle),
                            kTag, "Map background");
        const auto* data = static_cast<const uint8_t*>(mapped) + delta;
        if (esp_rom_crc32_le(0, data, bytes) != crc) {
            esp_partition_munmap(handle);
            ESP_LOGE(kTag, "Background does not match this firmware; restore paired assets");
            return ESP_ERR_INVALID_CRC;
        }
#ifdef SPRING_BACKGROUND_PSRAM
        const auto free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const auto largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        if (free < bytes + kPsramReserve || largest < bytes) {
            esp_partition_munmap(handle);
            ESP_LOGE(kTag, "Insufficient PSRAM budget for background (%u free, %u largest)",
                     static_cast<unsigned>(free), static_cast<unsigned>(largest));
            return ESP_ERR_NO_MEM;
        }
        auto* copy =
            static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!copy) {
            esp_partition_munmap(handle);
            ESP_LOGE(kTag, "PSRAM background allocation failed; no silent flash fallback");
            return ESP_ERR_NO_MEM;
        }
        ++memory.allocations;
        memcpy(copy, data, bytes);
        esp_partition_munmap(handle);
        if (esp_rom_crc32_le(0, copy, bytes) != crc) {
            heap_caps_free(copy);
            return ESP_ERR_INVALID_CRC;
        }
        *pixels = copy;
        memory.bytes = bytes;
        ESP_LOGI(kTag, "Verified background copied to PSRAM: %u bytes",
                 static_cast<unsigned>(bytes));
#else
        *pixels = data;
        ESP_LOGI(kTag, "Mapped verified background: %u bytes, no PSRAM pixel copy",
                 static_cast<unsigned>(bytes));
#endif
        return ESP_OK;
    }
    ESP_LOGE(kTag, "Background missing; flash the paired assets before this application");
    return ESP_ERR_NOT_FOUND;
}
}  // namespace

const uint8_t* LoadExternalBackground(const char* name, size_t bytes, uint32_t crc) {
    // Static initialization is thread-safe and attempts allocation only once per boot.
    static const uint8_t* pixels = [=] {
        const uint8_t* result = nullptr;
        const auto error = Map(name, bytes, crc, &result);
        memory.ready = error == ESP_OK;
        if (!memory.ready)
            ESP_LOGE(kTag, "Background unavailable (%d); show recovery screen", int(error));
        return result;
    }();
    return pixels;
}
BackgroundMemory GetBackgroundMemory() {
#ifdef SPRING_BACKGROUND_EMBEDDED
    return {0, 0, true};
#else
    return memory;
#endif
}
const char* BackgroundStorageMode() {
#ifdef SPRING_BACKGROUND_EMBEDDED
    return "embedded";
#elif defined(SPRING_BACKGROUND_PSRAM)
    return "psram";
#else
    return "flash";
#endif
}
}  // namespace spring_art
