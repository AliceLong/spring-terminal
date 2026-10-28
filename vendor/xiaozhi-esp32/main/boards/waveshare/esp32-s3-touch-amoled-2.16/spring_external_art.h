#pragma once
#include <cstddef>
#include <cstdint>

namespace spring_art {
// A separate lifetime mapping survives the voice Assets manager's remaps.
const uint8_t* LoadExternalBackground(const char* name, size_t bytes, uint32_t crc);
const char* BackgroundStorageMode();
struct BackgroundMemory {
    size_t bytes = 0;
    unsigned allocations = 0;
    bool ready = false;
};
BackgroundMemory GetBackgroundMemory();
}  // namespace spring_art
